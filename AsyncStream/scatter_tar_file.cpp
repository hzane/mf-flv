#include "stdafx.h"
#include <cassert>
#include <array>
#include "tarstream.hpp"
#include "contexts.hpp"

using scatter_tar_file_ptr = std::shared_ptr<scatter_tar_file>;
using std::make_shared;
using dbg::log;

scatter_tar_file::scatter_tar_file(){
  ZeroMemory(&status, sizeof(status));
}

scatter_tar_file::~scatter_tar_file(){
  assert(status.closed || status.failed || !status.opened);
}

void scatter_tar_file::fail_and_close(int64_t errcode) {
  lock_guard gd(lock);
  if (errcode) {
    status.error_code = errcode;
    status.failed = 1;
  }
  if (status.closed)
    return;
  (void)__super::close(); // return immediately
  status.closed = 1;
  log("fail-close %d, reason: %s\n", errcode, this->error_reason.c_str());
}


binary_task read_until_full(body_stream body, uint8_t* buf, size_t startat, size_t total_size) {
  binary_buff sbuf(buf + startat, total_size - startat);

  return body.read(sbuf, total_size - startat).then([body, buf, startat, total_size](body_read_task tread)->binary_task {
    try {
      auto readed = tread.get();
      auto nstartat = startat + readed;
      if (readed > 0 && nstartat < total_size) {
        return read_until_full(body, buf, nstartat, total_size);
      }
      return concurrency::task_from_result<int64_t>((int64_t)nstartat);
    } catch (std::exception e) {
      return task_from_result<int64_t>(e_tar_httpstream_ex | make_sure_negative);
    }
  });
}
void write_stream(scatter_tar_file_ptr pthis, body_stream body, uint64_t startat) {
  auto buf = std::shared_ptr<uint8_t>(new uint8_t[tar_slice_size]);
  // < 0, 0, >0
  read_until_full(body, buf.get(), 0, tar_slice_size).then([buf, pthis, startat, body](int64_t readed) {
      lock_guard gd(pthis->lock);
      if (readed == 0){
        if (pthis->status.stream_downloading){
          pthis->status.stream_downloading = 0;
          pthis->status.commited = 1;
          pthis->content_length = startat;  // true-length
        }
        body.close();
        return;
      }
      if (readed < 0){
        pthis->fail_and_close(readed);
        pthis->try_complete_read();
        return;
      }
      // readed isn't equal to tar_slice_size if it's the las block
      pthis->async_write(startat, (uint32_t)readed, buf.get())
          .then([buf](write_result) {      });

      // overlapped write
      if (readed == tar_slice_size) {
        write_stream(pthis, body, startat + readed);
      }
  });
}

void disable_random_access(scatter_tar_file_ptr pthis, uint64_t content_length){
  log("disable random access content-length : %I64u\n", content_length);
  lock_guard gd(pthis->lock);
  pthis->content_length = content_length;  // 0 : means unknown content_length
  pthis->status.opened = 1;
  if (content_length > 0) {
    auto bits = (content_length + tar_slice_size - 1) / tar_slice_size;
    pthis->null_slices.resize(bits, content_fixed_length);
    pthis->commited_slices.resize(bits, content_fixed_length);
  }
//  pthis->status.content_length = (content_length > 0) ? 1 : 0;    
}

void disable_random_access_then_write(scatter_tar_file_ptr pthis, uint64_t content_length, body_stream strm){
  disable_random_access(pthis, content_length);
  write_stream(pthis, strm, 0);
}

void enable_random_access(scatter_tar_file_ptr pthis, uint64_t content_length){
  log(L"enable random access content-length : %I64u\n", content_length);

  lock_guard gd(pthis->lock);
  pthis->content_length = content_length;
  pthis->status.opened = 1;
  pthis->status.random_access = 1;
  if (content_length > 0) {
    auto bits = (content_length + tar_slice_size - 1) / tar_slice_size;
    pthis->null_slices.resize(bits, content_fixed_length);
    pthis->commited_slices.resize(bits, content_fixed_length);
  }
//  pthis->status.content_length = (contentlength > 0) ? 1 : 0;
}

void enable_random_access_then_write(scatter_tar_file_ptr pthis, response_range rr, body_stream strm) {
  enable_random_access(pthis, rr.instance_size);
  write_stream(pthis, strm, rr.head);
}

void diagnose_http_headers(http_headers const&) {
}

resp_task do_request(std::wstring const&uri, request_range const&rng, http_method mthd, status_code accept) {
  auto cc = web::http::client::http_client_config();
  cc.set_timeout(utility::seconds(http_request_timeout));
  auto client = http_client(uri, cc);

  auto req = http_request(mthd);
  if(!rng.empty())  
    req.headers().add(L"range", request_range(0, tar_slice_size - 1).to_string());
  return client.request(req).then([accept](response_task tresp)->resp_task {
    try {
      // return task_from_result(http_response_noex(std::move(tresp.get())));
      auto resp = tresp.get();
      auto sc = resp.status_code();
      if (accept && sc != accept)
        return task_from_result(http_response_noex(utility::conversions::to_utf8string(resp.reason_phrase()).c_str(), sc | make_sure_negative));
      return task_from_result(http_response_noex(std::move(resp)));
    } catch (std::exception&e) {
      return task_from_result(http_response_noex(e.what(), e_tar_httpstream_ex));
    }
  });
}

int64_t scatter_tar_file::async_head(std::wstring const&uri) {
  lock_guard gd(lock);
  assert(status.heading == 0);
  url = uri;
  status.heading = 1;
  auto pthis = shared_from_this();

  do_request(url, request_range(0, tar_slice_size - 1), http_methods::HEAD, 0).then([pthis](http_response_noex resp) {
    lock_guard gd(pthis->lock);
    if (resp.code < 0) {
      pthis->error_reason = resp.reason;
      pthis->fail_and_close(resp.code);
    } else {
      resp._.body().close();

      auto sc = resp._.status_code();
      // transfer-encoding, content-length, content-range
      if (sc == web::http::status_codes::OK) {
        disable_random_access(pthis, resp._.headers().content_length());
        (void)pthis->async_download();
      } else if (sc == web::http::status_codes::PartialContent) {
        auto i = resp._.headers().find(L"content-range");
        auto rr = i != resp._.headers().end() ? response_range_decoder().decode(i->second) : response_range();
        enable_random_access(pthis, rr.instance_size);
      } else {
        pthis->fail_and_close(sc | make_sure_negative);
      }
    }
    pthis->status.headed = 1;
    pthis->try_complete_read();
    pthis->try_download_more();
  });

  return 0;
}
int64_t scatter_tar_file::async_download() {
  lock_guard gd(lock);
  if(status.random_access || !status.opened || status.closed || status.failed || status.stream_downloading)
    return e_tar_aborted;
  auto pthis = shared_from_this();

  do_request(url, request_range(), http_methods::GET, status_codes::OK).then([pthis](http_response_noex resp) {
      lock_guard gd(pthis->lock);
      if(resp.code >= 0){
        auto sc = resp._.status_code();
        if (sc != web::http::status_codes::OK){
          pthis->fail_and_close(sc | make_sure_negative);
          pthis->try_complete_read();  // complete pending readings if any
          resp._.body().close();  // close resp directly
        }
        pthis->content_length = resp._.headers().content_length();  //update content-length
        write_stream(pthis, resp._.body(), 0);
      }else{
        pthis->error_reason = resp.reason;
        pthis->fail_and_close(resp.code);
        pthis->try_complete_read();
      }
    });
  return 0;
}
int64_t scatter_tar_file::async_open(std::wstring const&uri) {
  lock_guard gd(lock);
  url = uri;
  assert(status.heading == 0 && status.headed == 0 && status.openning == 0);
  status.openning = 1;  // assert(status.openning == 0)
  auto pthis = shared_from_this();

  do_request(url, request_range(0, tar_slice_size - 1), http_methods::GET, 0).then([pthis](http_response_noex resp) {
      lock_guard gd(pthis->lock);
      int64_t reason = resp.code;
      if(resp.code >= 0){
        auto sc = resp._.status_code();
        reason = sc | make_sure_negative;
        if(sc == web::http::status_codes::OK){
          diagnose_http_headers(resp._.headers());
          disable_random_access_then_write(pthis, resp._.headers().content_length(), resp._.body());  // body closed by write-stream
          return;
        }else if(sc == web::http::status_codes::PartialContent) {
          auto i = resp._.headers().find(L"content-range");
          if (i != resp._.headers().end()) {
            auto rr = response_range_decoder().decode(i->second);
            return enable_random_access_then_write(pthis, rr, resp._.body());
          }
          reason = e_tar_null_content_range;
        } 
        resp._.body().close();
      }
      pthis->error_reason = resp.reason;
      pthis->fail_and_close(reason);
    });
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
  auto a = avail_at(read_op_context.start_position, read_op_context.expected);
  if (a != read_op_context.expected)
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

void scatter_tar_file::try_download_more(){
  lock_guard gd(lock);
  if (status.failed || status.closed || !status.random_access )
    return;
  if (status.progressive_downloadings)
    return;
  if (status.downloadings >= tar_max_downloadings || status.commited)
    return;

  // todo: stop when cached enough
  auto rng = first_unready_range(read_pointer, tar_max_chunk_size);
  if (rng.empty()){
    return;
  }
  log(L"try download slice %s\n", rng.to_string().c_str());
  for (auto i = rng.head; i <= rng.tail; ++i)
    null_slices.mask(i);

  status.progressive_downloadings = 1;
  ++status.downloadings;

  auto pthis = shared_from_this();
  async_download_range(rng * tar_slice_size).then([pthis, rng](response_range range){
      lock_guard gd(pthis->lock);
      --pthis->status.downloadings;
      pthis->status.progressive_downloadings = 0;
      if (range.error_code()) {
        if (++pthis->status.failed_count > max_failed_times)
          pthis->fail_and_close(range.error_code());
        for (auto i = rng.head; i <= rng.tail; ++i)
          pthis->null_slices.reset(i);
      } else {
        auto r = rng - range / tar_slice_size;
        for (auto i = r.head; i <= r.tail; ++i) {
          pthis->null_slices.reset(i);  // commited_slices already setted by write_stream
        }
        if (!r.empty() && !pthis->status.random_access) {
          pthis->status.commited = 1;
        }
      }
      pthis->try_download_more();  //needn't do try_complete_read;
    });
}

uint64_t scatter_tar_file::avail_at(uint64_t start, uint64_t expected_bytes){
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

download_task scatter_tar_file::async_download_range(request_range const& rng) {
  log(L"download-range %s\n", rng.to_string().c_str());
  auto pthis = shared_from_this();
  http_client client(url);
  auto req = http_request();
  req.headers().add(L"range", rng.to_string());
  return client.request(req).then([pthis](response_task taskresp)->download_task{
    try{
      auto resp = taskresp.get();
      auto sc = resp.status_code();
      if (sc != web::http::status_codes::PartialContent) {
        resp.body().close();
        return concurrency::task_from_result<response_range>(error_response_range(sc | make_sure_negative));
      }

      if (!resp.headers().has(L"content-range")){
        resp.body().close();
        return concurrency::task_from_result<response_range>(error_response_range(e_tar_null_content_range));
      }
      auto crs = resp.headers()[L"content-range"];
      auto rng = response_range_decoder().decode(crs);
      // transfer-encoding has been processed here
      write_stream(pthis, resp.body(), rng.head);  // will close body when complete
      return concurrency::task_from_result<response_range>(rng);
    } catch (web::http::http_exception &) {
      return concurrency::task_from_result<response_range>(error_response_range(e_tar_http_error));
    } catch (concurrency::task_canceled &) {
      return concurrency::task_from_result<response_range>(error_response_range(e_tar_aborted));
    } catch (...) {
      return concurrency::task_from_result<response_range>(error_response_range(e_tar_unknown));
    }
  });
}

