#include "stdafx.h"
#include <cassert>
#include <array>
#include "tarstream.hpp"
#include "contexts.hpp"
#include <cpprest/http_client.h>
#include <cpprest/streams.h>

using scatter_tar_file_ptr = std::shared_ptr<scatter_tar_file>;
using std::make_shared;

scatter_tar_file::scatter_tar_file(){
  ZeroMemory(&status, sizeof(status));
}

scatter_tar_file::~scatter_tar_file(){
  assert(status.closed || status.failed || !status.opened);
}
/*
int32_t scatter_tar_file::close(){
  lock_guard gd(lock);
  if (status.closed)
    return;
  auto v = __super::close();
  status.closed = 1;
  return v;
}
*/
void scatter_tar_file::fail_and_close(int32_t errcode) {
  lock_guard gd(lock);
  if (errcode) {
    status.error_code = errcode;
    status.failed = 1;
  }
  if (status.closed)
    return;
  (void)__super::close();
  status.closed = 1;
}
using web::http::client::http_client;
using web::http::http_request;
using web::http::http_response;
using response_task = concurrency::task<http_response>;

using concurrency::streams::istream;
using binary_stream = concurrency::streams::streambuf<uint8_t>;

void write_stream(scatter_tar_file_ptr pthis, istream body, uint64_t startat){
  binary_stream sbuf;
  body.read(sbuf, tar_slice_size).then([sbuf, pthis, startat, body](int readed) mutable{
      if(readed < 0){  // be carefull here
        //lock_guard gd(pthis->lock);
        // pthis->status.failed = 1;
        pthis->fail_and_close(readed);
        return;
      }else if(readed == 0){
        return;
      }
      // readed isn't equal to tar_slice_size if it's the las block
      auto buf = std::shared_ptr<uint8_t>(new uint8_t[readed]);
      auto cped = sbuf.scopy(buf.get(), readed);
      pthis->async_write(startat, cped, buf.get()).then([buf, pthis, body, startat](write_result writed){
          // assert(writed == cped)
        if (writed == tar_slice_size)  // only writed == tar_slice ,can we do the next write
          write_stream(pthis, body, startat + writed);
        });
    });
}
/*
download_task async_write_content(scatter_tar_file_ptr pthis, uint64_t start, istream body) {
  assert(start % tar_slice_size == 0);
  binary_stream sbuf;
  return body.read(sbuf, tar_slice_size).then([sbuf, pthis, start, body](int readed) mutable {
    if (readed < 0)
      return concurrency::task_from_result < response_range >(error_response_range(readed));
    if (readed == 0)
      return concurrency::task_from_result<response_range>(error_response_range(e_tar_http_eos));
    auto data = std::shared_ptr<uint8_t>(new uint8_t[tar_slice_size]);
    auto cped = sbuf.scopy(data.get(), tar_slice_size);
    auto nextstart = start + cped;
    return pthis->async_write(start, cped, data.get()).then([nextstart, body, pthis, data](write_result rlt) mutable -> download_task {
      if (rlt == tar_slice_size)  // only writed == tar_slice ,can we do the next write
        return async_write_content(pthis, nextstart, body);
      return concurrency::task_from_result<response_range>(error_response_range(e_tar_http_eos));
    });
  });
}
*/
void disable_random_access(scatter_tar_file_ptr pthis, uint64_t content_length, istream strm){
  lock_guard gd(pthis->lock);
  pthis->content_length = content_length;
  pthis->status.opened = 1;
  write_stream(pthis, strm, 0);
}

void enable_random_access(scatter_tar_file_ptr pthis, response_range const&rr, istream strm){
  lock_guard gd(pthis->lock);
  pthis->content_length = rr.instance_size;
  pthis->status.opened = 1;
  pthis->status.random_access = 1;
  write_stream(pthis, strm, rr.head);
}

int32_t scatter_tar_file::async_open(std::wstring const&uri){
  lock_guard gd(lock);
  url = uri;
  status.openning = 1;  // assert(status.openning == 0)
  auto pthis = shared_from_this();
  auto client = http_client(uri);
  auto req = http_request();
  req.headers().add(L"range", request_range(0, tar_slice_size -1).to_string());
  client.request(req).then([pthis](response_task tresp){
      lock_guard gd(pthis->lock);
      pthis->status.openning = 0;
      try{
        auto resp = tresp.get();
        auto sc = resp.status_code();
        if(sc == web::http::status_codes::OK){
          return disable_random_access(pthis, resp.headers().content_length(), resp.body());
        }else if(sc == web::http::status_codes::PartialContent) {
          response_range rr{1, 0, 0};// empty range
          if (resp.headers().has(L"content-range")){
            auto crs = resp.headers()[L"content-range"];
            rr = response_range_decoder().decode(crs);
          }
          if (rr.empty())
            pthis->fail_and_close(e_tar_null_content_range);
            //pthis->status.failed = 1;
          else return enable_random_access(pthis, rr, resp.body());
        }else{
          // pthis->status.failed = 1;
          pthis->fail_and_close(sc);
        }
        resp.body().close();
      }catch (web::http::http_exception &e) {
        pthis->error_reason = e.what();
        //pthis->status.failed = 1;
        pthis->fail_and_close(e_tar_http_error);
      } catch (concurrency::task_canceled &e) {
        pthis->error_reason = e.what();
        pthis->fail_and_close(e_tar_aborted);
        //pthis->status.failed = 1;
      } catch (...) {
        //pthis->status.failed = 1;
        pthis->fail_and_close(e_tar_unknown);
      }
    });
  return 0;
}

// allow overlapped writing
write_task scatter_tar_file::async_write(uint64_t start, uint32_t size, uint8_t const* data){
  assert(start % tar_slice_size == 0);
  auto pthis = shared_from_this();
  lock_guard gd(lock);
  ++status.writings;
  return __super::async_write(start, size, data).then([pthis, start](write_result wrreturn)->write_task {
    lock_guard gd(pthis->lock);
    --pthis->status.writings;
    pthis->update_write_pointer(start, wrreturn);
    return concurrency::task_from_result(wrreturn);
  });
}

read_task scatter_tar_file::async_read(uint32_t size, uint8_t* buf){
  lock_guard gd(lock);
  read_result x = 0;
  if (status.reading){
    x = e_tar_reading_overlapped;
  } else if (status.failed || status.closed) {
    x = e_tar_aborted;
  }
  if (x < 0)
    return concurrency::task_from_result(read_result{x});

  status.reading = 1;  // should be reset after task complete
  read_op_context.read_task_event = make_shared<read_task_completion_event>();
  auto v = read_task(*read_op_context.read_task_event);
  try_complete_read();
  return v;
}

// do when save data
// complete read if any
// try download more if cached data isn't enough
void scatter_tar_file::update_write_pointer(uint64_t start, int32_t result){
    assert(start % tar_slice_size == 0 );
    auto slice_idx_begin = start / tar_slice_size;
    if (result < 0) {  // failed for some reason, may be http error
      null_slices.reset((uint32_t)slice_idx_begin);  // return back
      return;
    }
    auto slice_idx_end = (start + result + tar_slice_size - 1) / tar_slice_size;
    lock_guard gd(lock);
    for(auto i = slice_idx_begin; i < slice_idx_end; ++i)
        commited_slices.mask((uint32_t)i);
    try_complete_read();
    try_download_more();
}

void scatter_tar_file::try_complete_read(){
  lock_guard gd(lock);
  if (!status.reading)
    return;
  assert(!status.tar_reading);
  auto a = avail_at(read_op_context.start_position, read_op_context.expected);
  if (a != read_op_context.expected)
    return;
  status.tar_reading = 1;
  __super::async_read(read_op_context.start_position, read_op_context.expected, read_op_context.buffer)
      .then([this](read_result rlt){
          lock_guard gd(lock);
          status.tar_reading = 0;
          update_read_pointer(rlt);
        });
}

void scatter_tar_file::update_read_pointer(read_result rlt){
  //  lock_guard gd(lock);
  if(rlt > 0 )
    read_pointer += rlt;
  read_op_context.read_task_event->set(rlt);  // notify task complete
  read_op_context.read_task_event = nullptr;  // release task_completion_event
  status.reading = 0;
  try_download_more();
}

void scatter_tar_file::try_download_more(){
  lock_guard gd(lock);
  if (status.downloadings >= tar_max_downloadings || status.commited)
    return;
  auto rng = first_range_unavail_from(read_pointer, tar_max_chunk_size);
  if (rng.empty()){
    return;
  }
  for (auto i = rng.head; i < rng.tail; ++i)
    null_slices.mask((uint32_t)i);

  ++status.downloadings;
  auto sz = rng.size() * tar_slice_size;
  auto buf = std::shared_ptr<uint8_t>(new uint8_t[(size_t)sz]);
  auto pthis = shared_from_this();
  async_download_range(rng * tar_slice_size, buf.get(), (uint32_t)sz).then([buf, pthis, rng](response_range range){
      lock_guard gd(pthis->lock);
      --pthis->status.downloadings;
      auto r = rng - range / tar_slice_size;
      for (auto i = r.head; i <= r.tail; ++i) {
        pthis->null_slices.reset((uint32_t)i);  // commited_slices already setted by write_stream
      }
    });
}

uint64_t scatter_tar_file::avail_at(uint64_t start, uint32_t expected_bytes){
  lock_guard gd(lock);
  auto idx = start / tar_slice_size;
  auto end = (start + expected_bytes + tar_slice_size - 1) / tar_slice_size;
  auto slices = commited_slices.continues((uint32_t)idx, (uint32_t)end);
  auto true_end = min((idx + slices) * tar_slice_size, start + expected_bytes);
  return true_end - start;
}

request_range scatter_tar_file::first_range_unavail_from(uint64_t start, uint32_t maxbytes){
  lock_guard gd(lock);
  auto idx = start / tar_slice_size;
  auto s = null_slices.first_null((uint32_t)idx);
  auto end = s + (maxbytes + tar_slice_size - 1) / tar_slice_size;
  auto v = null_slices.continues_null(s, (uint32_t)end);
  return request_range{s , s+v -1};
}
download_task scatter_tar_file::async_download_range(request_range const& rng, uint8_t* buf, uint32_t len){
  auto pthis = shared_from_this();
  http_client client(url);
  auto req = http_request();
  req.headers().add(L"range", rng.to_string());
  return client.request(req).then([pthis](response_task taskresp)->download_task{
    try{
      auto resp = taskresp.get();
      auto sc = resp.status_code();
      if (sc != web::http::status_codes::PartialContent) {
        return concurrency::task_from_result<response_range>(error_response_range(sc | make_sure_negative));
      }
      // auto cl = resp.headers().content_length();
      if (!resp.headers().has(L"content-range"))
        return concurrency::task_from_result<response_range>(error_response_range(e_tar_null_content_range));
      auto crs = resp.headers()[L"content-range"];
      auto rng = response_range_decoder().decode(crs);
      // transfer-encoding has been processed here
      auto body = resp.body();
      write_stream(pthis, body, rng.head);
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