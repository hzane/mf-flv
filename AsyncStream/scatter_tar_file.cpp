#include "stdafx.h"
#include <cassert>
#include <array>
#include "asynchttpstream/contexts.hpp"
#include "asynchttpstream/scatter_tar_file.hpp"

using this_t = std::shared_ptr<scatter_tar_file>;
using dbg::log;

scatter_tar_file::~scatter_tar_file(){
  assert(status.closed || status.failed || !status.opened);
}

void scatter_tar_file::fail_and_close(int64_t errcode, const char* reason) {
  lock_guard gd(lock);
  if (errcode) {
    status.error_code = errcode;
    status.failed = 1;
    if (reason) this->reason = utility::conversions::to_string_t(reason);
  }
  if (status.closed)
    return;
  (void)__super::close(); // return immediately
  status.closed = 1;
  try_complete_read();
  log(L"fail-close %d, reason: %s\n", errcode, this->reason.c_str());
}

binary_task read_until_full(body_stream body, uint8_t* buf, size_t startat, size_t total_size) {
  binary_buff sbuf(buf + startat, total_size - startat);
  return body.read(sbuf, total_size - startat)
      .then([body, buf, startat, total_size](body_read_task tread)->binary_task {
    try {
      auto readed = tread.get();
      auto nstartat = startat + readed;
      if (readed > 0 && nstartat < total_size) // no more data
        return read_until_full(body, buf, nstartat, total_size);

      return concurrency::task_from_result<int64_t>((int64_t)nstartat);
    } catch (std::exception e) {
      return task_from_result<int64_t>(e_tar_httpstream_ex | make_sure_negative);
    }
  });
}

void write_stream(this_t pthis, body_stream body, uint64_t startat, uint64_t already_readed) {
  auto buf = std::shared_ptr<uint8_t>(new uint8_t[tar_slice_size]);

  // < 0 : failed , 0 : end of stream, >0 : bytes
  read_until_full(body, buf.get(), 0, tar_slice_size).then([buf, pthis, startat, body, already_readed](int64_t readed) {
      lock_guard gd(pthis->lock);
      if (readed < tar_slice_size) {  // end of stream of error
        body.close();
      }
      if (readed == 0){
        log("write-stream %u bytes\n", already_readed);
        return;
      }
      if (readed < 0)
        return pthis->fail_and_close(readed);
      auto curreaded = already_readed + readed;
      // readed isn't equal to tar_slice_size if it's the last block
      // ignore write_result
      pthis->async_write(startat, readed, buf.get())
          .then([buf](write_result) {      });  // use lambda to hold buf

      // overlapped write, not complete yet
      if (readed == tar_slice_size) {
        write_stream(pthis, body, startat + readed, curreaded);
      }
  });
}

void diagnose_http_headers(http_headers const&) {
}

static wchar_t const* chrome_agent = L"Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/32.0.1664.3 Safari/537.36";

void pretend_chrome(http_headers headers, wchar_t const*host, wchar_t const*referer) {
  headers.add(L"accept", L"*/*");
  headers.add(L"accept-encoding", L"identity");  // not accept gzip;deflate;compress
  headers.add(L"accept-language", L"en-US,en;q=0.8,zh;q=0.6,zh-CN;q=0.4,zh-TW;q=0.2");
  headers.add(L"connection", L"close");  // we dont't reuse any connection
  headers.add(L"DNT", L"1");  // dont track
  if (host) headers.add(L"host", host);
  if (referer) headers.add(L"referer", referer);
  headers.add(L"user-agent", chrome_agent);
}

using utility::conversions::to_utf8string;

resp_task do_request(std::wstring const&uri, request_range const&rng, http_method mthd, status_code accept) {
  auto cc = web::http::client::http_client_config();
  cc.set_timeout(utility::seconds(http_request_timeout));
  auto client = http_client(uri, cc);

  auto req = http_request(mthd);
  pretend_chrome(req.headers(), nullptr, L"http://weibo.com");
  if(!rng.empty())
    req.headers().add(L"range", rng.to_string());
  return client.request(req).then([accept](response_task tresp)->resp_task {
    try {
      auto resp = tresp.get();
      auto sc = resp.status_code();
      if (accept && sc != accept) {
        log(L"%d %s\n", sc, resp.reason_phrase().c_str());
        return task_from_result(http_response_noex(to_utf8string(resp.reason_phrase()).c_str(), sc | make_sure_negative));
      }
      return task_from_result(http_response_noex(std::move(resp)));
    } catch (std::exception&e) {  // timeout, http-exeception, runtime-ex
      log("do-request ex: %s\n", e.what());
      return task_from_result(http_response_noex(e.what(), e_tar_httpstream_ex));
    }
  });
}

// we'v already make sure the server supports range request
// content_length, 0: unknown, other: whole content length

int64_t scatter_tar_file::async_open(std::wstring const&uri, uint64_t content_length) {
  this->content_length = content_length;
  this->url = uri;
  ZeroMemory(&status, sizeof(status));
  if (content_length) {
    auto bits = (content_length + tar_slice_size - 1) / tar_slice_size;
    null_slices.resize(bits, content_fixed_length);
    commited_slices.resize(bits, content_fixed_length);
  }
  try_download_more();
  return 0;
}

// allow overlapped writing
write_task scatter_tar_file::async_write(uint64_t start, uint64_t size, uint8_t const* data) {
  assert(start % tar_slice_size == 0);
  auto pthis = shared_from_this();
  lock_guard gd(lock);
  if (status.failed || status.closed)
    return task_from_result(status.error_code);  // ignore all write if file closed
  ++status.writings;
  return __super::async_write(start, size, data).then([pthis, start](write_result wrreturn)->write_task {
    if (wrreturn != tar_slice_size) {
      if (pthis->content_length == 0 || pthis->content_length == UINT64_MAX) {
        pthis->content_length = start + wrreturn;
      }
      log("scatter-tar-file write the last block\n");
    }
    lock_guard gd(pthis->lock);
    --pthis->status.writings;
    pthis->update_write_pointer(start, wrreturn);
    return concurrency::task_from_result(wrreturn);
  });
}

read_task scatter_tar_file::async_read(uint64_t size, uint8_t* buf) {
  lock_guard gd(lock);
  read_result x = 0;
  if (status.reading){
    x = e_tar_reading_overlapped | make_sure_negative;
  } else if (status.failed || status.closed) {
    x = e_tar_aborted | make_sure_negative;
  }
  if (x < 0)
    return concurrency::task_from_result<read_result>(x);
  if (content_length && read_pointer >= content_length)
    return task_from_result<read_result>(e_tar_eof);

  status.reading = 1;  // should be reset after task complete
  read_op_context.read_task_event = make_shared<read_task_completion_event>();
  read_op_context.expected = size;
  read_op_context.start_position = read_pointer;
  read_op_context.buffer = buf;
  auto v = read_task(*read_op_context.read_task_event);
  try_complete_read();
  return v;
}

// called when save data
// no more than one slice a time
void scatter_tar_file::update_write_pointer(uint64_t start, int64_t writed) {
  assert(start % tar_slice_size == 0);
  auto slice_idx_begin = start / tar_slice_size;
  lock_guard gd(lock);
  if (writed < 0) {  // failed for some reason, may be http error
    null_slices.reset((uint32_t)slice_idx_begin);  // return back
    return;
  }
  status.wroten_bytes += writed;
  auto slice_idx_end = (start + writed + tar_slice_size - 1) / tar_slice_size;
  for (auto i = slice_idx_begin; i < slice_idx_end; ++i)
    commited_slices.mask((uint32_t)i);
  try_complete_read();
  try_download_more();
}

void scatter_tar_file::try_complete_read(){
  lock_guard gd(lock);
  if (status.tar_reading)
    return;  // there is a pending file reading
  if (!status.reading)  // be reset in update-read-pointer
    return;  // there is a reading request

  if (status.failed || status.closed) {
    update_read_pointer(status.error_code | make_sure_negative);
    return;
  }
  assert(!status.tar_reading);
  // a <= 0 if no available content
  auto a = avail_bytes_from(read_op_context.start_position, read_op_context.expected);
  if (a < (int64_t)read_op_context.expected)
    return;  // wait fore more data
  status.tar_reading = 1;
  __super::async_read(read_op_context.start_position, read_op_context.expected, read_op_context.buffer)
      .then([this](read_result readed){
          lock_guard gd(lock);
          status.tar_reading = 0;
          update_read_pointer(readed);
        });
}

void scatter_tar_file::update_read_pointer(read_result readed){
  assert(read_pointer == read_op_context.start_position);
  //  lock_guard gd(lock); already locked before
  if (readed > 0 && read_pointer == read_op_context.start_position)  // seek will change read-pointer
    read_pointer += readed;
  status.reading = 0;
  read_op_context.read_task_event->set(readed);  // notify task complete
  read_op_context.read_task_event = nullptr;  // release task_completion_event
  try_download_more();
}

// when response status isn't supported
// throw an exception
void when_response_failed(this_t pthis, int64_t errcode, request_range r) {
  for (auto i = r.head; i <= r.tail; ++i)  // revert downloading flags
    pthis->null_slices.reset(i);
  if (++pthis->status.failed_count > max_failed_times)
    pthis->fail_and_close(errcode | make_sure_negative);
}

// unrecognized content-range
void when_response_ok(this_t pthis, http_response &resp, request_range r) {
  auto i = resp.headers().find(L"content-range");
  auto rr = i != resp.headers().end() ? response_range_decoder().decode(i->second) : response_range();
  if (rr.empty()) {
    pthis->fail_and_close(e_tar_null_content_range);
    resp.body().close();
  } else
    write_stream(pthis, resp.body(), rr.head, 0);
  if (rr.instance_size && !pthis->content_length) {
    pthis->content_length = rr.instance_size;
    auto bits = (rr.instance_size + tar_slice_size - 1) / tar_slice_size;
    pthis->null_slices.resize(bits, content_fixed_length);
    pthis->commited_slices.resize(bits, content_fixed_length);
  }

  // return back undownloaded slices, normally this range is empty
  auto left = r - rr / tar_slice_size;
  for (auto i = left.head; i <= left.tail; ++i)
    pthis->null_slices.reset(i);
  pthis->try_download_more();
}

void scatter_tar_file::try_download_more(){
  lock_guard gd(lock);
  if (status.failed || status.closed )
    return;
  if (status.progressive_downloading)
    return;
  if (status.downloadings >= tar_max_downloadings || status.content_ready)
    return;

  // if we cached enough
  auto x = avail_bytes_from(read_pointer, tar_max_cache_size  + tar_max_chunk_size);
  if (x > (int64_t)tar_max_cache_size)
    return;

  // slice range
  auto rng = first_unready_range(read_pointer, tar_max_chunk_size);
  if (rng.empty())
    rng = first_unready_range(0, tar_max_chunk_size);
  if (rng.empty()) {
    return;
  }
  log(L"scatter-tar-file try download slice %s\n", rng.to_string().c_str());
  for (auto i = rng.head; i <= rng.tail; ++i)
    null_slices.mask(i);

  status.progressive_downloading = 1;
  ++status.downloadings;

  auto pthis = shared_from_this();
  do_request(url, rng * tar_slice_size, http_methods::GET, status_codes::PartialContent).then([pthis, rng](http_response_noex resp) {
    lock_guard g(pthis->lock);
    --pthis->status.downloadings;
    pthis->status.progressive_downloading = 0;
    if (resp.code)
      when_response_failed(pthis, resp.code | make_sure_negative, rng);
    else
      when_response_ok(pthis, resp._, rng);
    pthis->try_download_more();
  });
}

int64_t scatter_tar_file::avail_bytes_from(uint64_t start, uint64_t expected_bytes) {
  lock_guard gd(lock);
  auto idx = start / tar_slice_size;
  auto end = (start + expected_bytes + tar_slice_size - 1) / tar_slice_size;
  auto slices = commited_slices.continues(idx, end);
  auto true_end = min((idx + slices) * tar_slice_size, start + expected_bytes);
  if (content_length)
    true_end = min(true_end, content_length);
  return true_end - start;
}

// slice range
request_range scatter_tar_file::first_unready_range(uint64_t start, uint64_t maxbytes) {
  lock_guard gd(lock);
  auto idx = start / tar_slice_size;
  auto s = null_slices.first_null(idx);
  auto end = s + (maxbytes + tar_slice_size - 1) / tar_slice_size;
  auto v = null_slices.continues_null(s, end);
  return request_range{s , s+v -1};
}

uint64_t scatter_tar_file::seek(uint64_t pos) {
  lock_guard g(lock);

  read_pointer = pos;
  try_download_more();
  return pos;
}

scatter_tar_file_handler::scheme_task
scatter_tar_file_handler::async_check(std::wstring const&url) {
  return do_request(url, request_range(0, tar_slice_size - 1), http_methods::HEAD, status_codes::PartialContent)
    .then([url](http_response_noex resp) ->scheme_task {
      scheme_check_result v;
      v.error = resp.code < 0 ? (int32_t)(resp.code | make_sure_negativei32) : (int32_t)resp.code;
      v.reason = utility::conversions::to_utf16string(resp.reason);
      v.url = url;
      if (resp.code)
        return task_from_result(v);
      auto i = resp._.headers().find(L"content-range");
      if (i != resp._.headers().end()) {
        auto r = response_range_decoder().decode(i->second);
        if (!r.empty())
          v.content_length = r.instance_size;
      }
      v.content_type = resp._.headers().content_type();
      resp._.body().close();
      return task_from_result(v);
  });
}
