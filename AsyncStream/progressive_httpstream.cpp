#include "stdafx.h"
#include "progressive_httpstream.hpp"

using guard = std::lock_guard<std::recursive_mutex>;
using this_t = std::shared_ptr<progressive_httpstream>;
using dbg::log;
read_task progressive_httpstream::async_read(uint64_t size, uint8_t *buf) {
  guard g(lock);
  read_result x = 0;
  if (status.reading)
    x = e_tar_reading_overlapped;
  else if (status.failed || status.closed)
    x = e_tar_aborted;
  if (x < 0)
    return task_from_result<read_result>(x);
  if (status.has_content_length && content_length && read_pointer >= content_length)
    return task_from_result<read_result>(e_tar_eof);
  status.reading = 1;
  read_op_context.read_task_event = make_shared<read_task_completion_event>();
  read_op_context.expected = size;
  read_op_context.start_position = read_pointer;
  read_op_context.buffer = buf;
  auto v = read_task(*read_op_context.read_task_event);
  try_complete_read();
  return v;
}

void progressive_httpstream::try_complete_read() {
  guard g(lock);
  if (!status.reading)
    return;
  if (status.failed || status.closed)
    return update_read_pointer(status.error_code | make_sure_negative);
  assert(!status.tar_reading);

  auto pthis = shared_from_this();
  status.tar_reading = 1;
  __super::async_read(read_op_context.start_position, read_op_context.expected, read_op_context.buffer).then(
    [pthis](read_result readed) {
      guard g(pthis->lock);
      pthis->status.tar_reading = 0;
      pthis->update_read_pointer(readed);
  });
}
void progressive_httpstream::update_read_pointer(read_result readed) {
  if (readed > 0)
    read_pointer += readed;
  status.reading = 0;
  read_op_context.read_task_event->set(readed);
  read_op_context.read_task_event = nullptr;
  try_download_more();
}
resp_task do_request(std::wstring const&uri, request_range const&rng, http_method mthd, status_code accept);
void when_response_failed(this_t pthis, int64_t errcode, request_range r);
void when_response_ok(this_t pthis, http_response &resp, request_range r);
binary_task read_until_full(body_stream body, uint8_t* buf, size_t startat, size_t total_size);

void progressive_httpstream::try_download_more() {
  guard g(lock);
  assert(!status.streaming);
  if (status.failed || status.closed)
    return;
  if (status.progressive_downloading)
    return;
  if (status.downloadings >= tar_max_downloadings || status.content_ready)
    return;
  auto r = first_unready_range(read_pointer, tar_max_chunk_size);
  if (r.empty())
    r = first_unready_range(0, tar_max_chunk_size);
  if (r.empty()) {
    status.content_ready = 1;
    return;
  }
  for (auto i = r.head; i <= r.tail; ++i)
    null_slices.mask(i);
  status.progressive_downloading = 1;
  ++status.downloadings;
  auto pthis = shared_from_this();
  do_request(url, r, http_methods::GET, status_codes::PartialContent).then([pthis, r](http_response_noex resp) {
    guard g(pthis->lock);
    --pthis->status.downloadings;
    pthis->status.progressive_downloading = 0;
    if (resp.code)
      when_response_failed(pthis, resp.code | make_sure_negative, r);
    else
      when_response_ok(pthis, resp._, r);
    pthis->try_download_more();
  });
}

void when_response_failed(this_t pthis, int64_t errcode, request_range r) {
  if (++pthis->status.failed_count > max_failed_times)
    pthis->fail_and_close(errcode | make_sure_negative);
  for (auto i = r.head; i <= r.tail; ++i)  // revert downloading flags
    pthis->null_slices.reset(i);
}
void write_stream(this_t pthis, body_stream body, uint64_t startat) {
  auto buf = std::shared_ptr<uint8_t>(new uint8_t[tar_slice_size]);
  // < 0, 0, >0
  read_until_full(body, buf.get(), 0, tar_slice_size).then([buf, pthis, startat, body](int64_t readed) {
    lock_guard gd(pthis->lock);
    if (readed == 0) {
      if (pthis->status.stream_downloading) {
        pthis->status.stream_downloading = 0;
        pthis->status.content_ready = 1;
        pthis->content_length = startat;  // true-length
      }
      body.close();
      return;
    }
    if (readed < 0) {
      pthis->fail_and_close(readed);
      pthis->try_complete_read();
      return;
    }
    // readed isn't equal to tar_slice_size if it's the las block
    pthis->async_write(startat, (uint32_t)readed, buf.get())
      .then([buf](write_result) {});

    // overlapped write
    if (readed == tar_slice_size) {
      write_stream(pthis, body, startat + readed);
    }
  });
}
void when_response_ok(this_t pthis, http_response &resp, request_range r) {
  auto i = resp.headers().find(L"content-range");
  auto rr = i != resp.headers().end() ? response_range_decoder().decode(i->second) : response_range();
  if (rr.empty()) {
    pthis->fail_and_close(e_tar_null_content_range);
    resp.body().close();
  } else
    write_stream(pthis, resp.body(), rr.head);
  auto left = r - rr / tar_slice_size;
  for (auto i = left.head; i <= left.tail; ++i)
    pthis->null_slices.reset(i);
  pthis->try_download_more();
}
void progressive_httpstream::fail_and_close(int64_t errcode, const char*reason) {
  lock_guard gd(lock);
  if (errcode) {
    status.error_code = errcode;
    status.failed = 1;
    if(reason ) error_reason = utility::conversions::utf8_to_utf16(reason);
  }
  if (status.closed)
    return;
  (void)__super::close(); // return immediately
  status.closed = 1;
  log(L"fail-close %d, reason: %s\n", errcode, this->error_reason.c_str());
}

// allow overlapped writing
write_task progressive_httpstream::async_write(uint64_t start, uint64_t size, uint8_t const* data) {
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
// do when save data
// complete read if any
// try download more if cached data isn't enough
// no more than one slice a time
void progressive_httpstream::update_write_pointer(uint64_t start, int64_t writed) {
  //  log("update write pointer %I64u, return : %d\n", start, result);
  assert(start % tar_slice_size == 0);
  auto slice_idx_begin = start / tar_slice_size;
  guard gd(lock);
  if (writed < 0) {  // failed for some reason, may be http error
    null_slices.reset(slice_idx_begin);  // return back
    return;
  }
  status.wroten_bytes += writed;
  auto slice_idx_end = (start + writed + tar_slice_size - 1) / tar_slice_size;
  for (auto i = slice_idx_begin; i < slice_idx_end; ++i)
    commited_slices.mask(i);
  try_complete_read();
  try_download_more();
}

request_range progressive_httpstream::first_unready_range(uint64_t start, uint64_t maxbytes) {
  guard gd(lock);
  auto idx = start / tar_slice_size;
  auto s = null_slices.first_null(idx);
  auto end = s + (maxbytes + tar_slice_size - 1) / tar_slice_size;
  auto v = null_slices.continues_null(s, end);
  return request_range{s, s + v - 1};
}
