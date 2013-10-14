#include "stdafx.h"
#include <cassert>
#include <array>
#include "tarstream.hpp"
#include "contexts.hpp"

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

void write_stream(this_t pthis, body_stream body, uint64_t startat) {
  auto buf = std::shared_ptr<uint8_t>(new uint8_t[tar_slice_size]);
  // < 0, 0, >0
  read_until_full(body, buf.get(), 0, tar_slice_size).then([buf, pthis, startat, body](int64_t readed) {
      lock_guard gd(pthis->lock);
      if (readed == 0){
        body.close();
        return;
      }
      if (readed < 0)
        return pthis->fail_and_close(readed);

      // readed isn't equal to tar_slice_size if it's the las block
      // ignore write_result
      pthis->async_write(startat, (uint32_t)readed, buf.get())
          .then([buf](write_result) {      });

      // overlapped write, not complete yet
      if (readed == tar_slice_size) {
        write_stream(pthis, body, startat + readed);
      }
  });
}
void diagnose_http_headers(http_headers const&) {
}

using utility::conversions::to_utf8string;

resp_task do_request(std::wstring const&uri, request_range const&rng, http_method mthd, status_code accept) {
  auto cc = web::http::client::http_client_config();
  cc.set_timeout(utility::seconds(http_request_timeout));
  auto client = http_client(uri, cc);

  auto req = http_request(mthd);
  if(!rng.empty())
    req.headers().add(L"range", rng.to_string());
  return client.request(req).then([accept](response_task tresp)->resp_task {
    try {
      auto resp = tresp.get();
      auto sc = resp.status_code();
      if (accept && sc != accept)
        return task_from_result(http_response_noex(to_utf8string(resp.reason_phrase()).c_str(), sc | make_sure_negative));
      return task_from_result(http_response_noex(std::move(resp)));
    } catch (std::exception&e) {
      return task_from_result(http_response_noex(e.what(), e_tar_httpstream_ex));
    }
  });
}
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
  ++status.writings;
  return __super::async_write(start, size, data).then([pthis, start](write_result wrreturn)->write_task {
    log("write [%I64u, %d], all: %I64u\n", start, wrreturn, pthis->status.wroten_bytes);
    if (wrreturn != tar_slice_size) {
      log("the last block\n");
    }
    lock_guard gd(pthis->lock);
    --pthis->status.writings;
    pthis->update_write_pointer(start, wrreturn);
    return concurrency::task_from_result(wrreturn);
  });
}

read_task scatter_tar_file::async_read(uint64_t size, uint8_t* buf) {
  log("read %d\n", size);
  lock_guard gd(lock);
  read_result x = 0;
  if (status.reading){
    x = e_tar_reading_overlapped | make_sure_negative;
  } else if (status.failed || status.closed) {
    x = e_tar_aborted | make_sure_negative;
  }
  if (x < 0)
    return concurrency::task_from_result<read_result>(x);
  if (content_length && read_pointer == content_length)
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

// do when save data
// complete read if any
// try download more if cached data isn't enough
// no more than one slice a time
void scatter_tar_file::update_write_pointer(uint64_t start, int64_t writed) {
//  log("update write pointer %I64u, return : %d\n", start, result);
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
  if (!status.reading)  // be reset in update-read-pointer
    return;
  if (status.failed || status.closed) {
    update_read_pointer(status.error_code | make_sure_negative);
    return;
  }
  assert(!status.tar_reading);
  auto a = avail_bytes_from(read_op_context.start_position, read_op_context.expected);
  if (a < read_op_context.expected)
    return;  // wait fore more data
  log("try complete read\n");
  status.tar_reading = 1;
  __super::async_read(read_op_context.start_position, read_op_context.expected, read_op_context.buffer)
      .then([this](read_result readed){
          lock_guard gd(lock);
          status.tar_reading = 0;
          update_read_pointer(readed);
        });
}

void scatter_tar_file::update_read_pointer(read_result readed){
  log("update read pointer %d\n", readed);
  //  lock_guard gd(lock); already locked before
  if(readed > 0 )
    read_pointer += readed;
  status.reading = 0;
  read_op_context.read_task_event->set(readed);  // notify task complete
  read_op_context.read_task_event = nullptr;  // release task_completion_event
  try_download_more();
}

void when_response_failed(this_t pthis, int64_t errcode, request_range r) {
  for (auto i = r.head; i <= r.tail; ++i)  // revert downloading flags
    pthis->null_slices.reset(i);
  if (++pthis->status.failed_count > max_failed_times)
    pthis->fail_and_close(errcode | make_sure_negative);
}
void when_response_ok(this_t pthis, http_response &resp, request_range r) {
  auto i = resp.headers().find(L"content-range");
  auto rr = i != resp.headers().end() ? response_range_decoder().decode(i->second) : response_range();
  if (rr.empty()) {
    pthis->fail_and_close(e_tar_null_content_range);
    resp.body().close();
  } else 
    write_stream(pthis, resp.body(), rr.head);
  if (rr.instance_size && !pthis->content_length) {
    pthis->content_length = rr.instance_size;
    auto bits = (rr.instance_size + tar_slice_size - 1) / tar_slice_size;
    pthis->null_slices.resize(bits, content_fixed_length);
    pthis->commited_slices.resize(bits, content_fixed_length);
  }
    
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

  // todo: stop when cached enough
  auto rng = first_unready_range(read_pointer, tar_max_chunk_size);
  if (rng.empty())
    rng = first_unready_range(0, tar_max_chunk_size);
  if (rng.empty()) {    
    return;
  }
  log(L"try download slice %s\n", rng.to_string().c_str());
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

uint64_t scatter_tar_file::avail_bytes_from(uint64_t start, uint64_t expected_bytes) {
  lock_guard gd(lock);
  auto idx = start / tar_slice_size;
  auto end = (start + expected_bytes + tar_slice_size - 1) / tar_slice_size;
  auto slices = commited_slices.continues(idx, end);
  auto true_end = min((idx + slices) * tar_slice_size, start + expected_bytes);
  if (content_length)
    true_end = min(true_end, content_length);
  return true_end - start;
}

request_range scatter_tar_file::first_unready_range(uint64_t start, uint64_t maxbytes) {
  lock_guard gd(lock);
  auto idx = start / tar_slice_size;
  auto s = null_slices.first_null(idx);
  auto end = s + (maxbytes + tar_slice_size - 1) / tar_slice_size;
  auto v = null_slices.continues_null(s, end);
  return request_range{s , s+v -1};
}
