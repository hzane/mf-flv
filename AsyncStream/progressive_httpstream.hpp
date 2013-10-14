#pragma once
#include <string>
#include "tar_file.hpp"
#include "contexts.hpp"
#include "bitfield.hpp"
#include "ranges.hpp"

struct ihttp_stream{
  virtual read_task async_read(uint64_t size, uint8_t *buf) = 0;
  virtual int64_t   seek(uint64_t offset)                   = 0;
};
struct progressive_httpstream: public ihttp_stream, tar_file, std::enable_shared_from_this<progressive_httpstream> {
  read_task async_read(uint64_t size, uint8_t *buf);
  int64_t   open(wchar_t const*uri, uint64_t content_length);
  int64_t   seek(uint64_t offset);

  write_task async_write(uint64_t startat, uint64_t size, uint8_t const*data);
  void fail_and_close(int64_t err, const char* reason = nullptr);
  void try_complete_read();
  void try_download_more();
  void update_read_pointer(read_result readed);
  void     update_write_pointer(uint64_t start, int64_t result);  // called when write complete
  request_range first_unready_range(uint64_t startat, uint64_t maxrangesize);

  std::wstring           url;
  uint64_t               content_length = 0;
  read_operation_context read_op_context;
  bitfield               null_slices;   // 0 : unalloced, 1 : alloced or commited block
  bitfield               commited_slices; // 0 : undownloaded, 1: downloaded block
  uint64_t               read_pointer   = 0;

  struct {
    uint64_t wroten_bytes;
    int64_t error_code;
    uint8_t reading                  : 1;
    uint8_t tar_reading              : 1;
    uint8_t heading                  : 1;
    uint8_t headed                   : 1;
    uint8_t openning                 : 1;
    uint8_t opened                   : 1;
    uint8_t stream_downloading       : 1;
    uint8_t progressive_downloading  : 1;
    uint8_t downloadings             : 3;
    uint8_t content_ready            : 1;
    uint8_t closed                   : 1;
    uint8_t failed                   : 1;
    uint8_t streaming                : 1;
    uint8_t has_content_length       : 1;
uint8_t                              : 0;
    uint8_t failed_count;
    uint8_t writings;
  }status;
  std::wstring error_reason;
};
