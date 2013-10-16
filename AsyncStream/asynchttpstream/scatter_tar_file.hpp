#pragma once
#include <string>
#include "tar_file.hpp"
#include "bitfield.hpp"
#include "contexts.hpp"
#include "ranges.hpp"

using download_task = concurrency::task<response_range>;

struct scatter_tar_file : public tar_file, std::enable_shared_from_this<scatter_tar_file> {
  //  scatter_tar_file();
  ~scatter_tar_file();
//  int64_t async_head(std::wstring const&url);
  int64_t   async_open(std::wstring const&url, uint64_t content_length);  //0, -1 : unknown length
  read_task async_read(uint64_t size, uint8_t* buffer); // streaming read at read_pointer
  uint64_t  seek(uint64_t pos);

  write_task    async_write(uint64_t startat, uint64_t size, uint8_t const* data);
  int64_t       avail_bytes_from(uint64_t startat, uint64_t expected);  // bytes
  request_range first_unready_range(uint64_t start, uint64_t maxcount);  // bytes
  void          fail_and_close(int64_t errcode, const char*reason = 0);
  void          try_complete_read();  // if a read pending
  void          try_download_more();  // if cached data isn't enough
  void          update_write_pointer(uint64_t start, int64_t result);  // called when write complete
  void          update_read_pointer(read_result result);  // called when read complete

  read_operation_context read_op_context;
  bitfield               null_slices;     // 0 : unalloced, 1 : alloced or commited block
  bitfield               commited_slices; // 0 : undownloaded, 1: downloaded block
  uint64_t               read_pointer   = 0;
  uint64_t               content_length = 0;
  std::wstring           url;
  std::wstring           reason;          // if error_code != 0
  struct {
    uint64_t wroten_bytes;
//    uint64_t seek_pointer;
    int64_t error_code;
    uint8_t reading           : 1;
    uint8_t tar_reading       : 1;
    uint8_t heading           : 1;
    uint8_t headed            : 1;
    uint8_t openning : 1;
    uint8_t opened : 1;
    uint8_t progressive_downloading : 1;
    uint8_t completing_reading : 1;
    uint8_t downloadings      : 3;
    uint8_t content_ready     : 1;
    uint8_t closed            : 1;
    uint8_t failed            : 1;
//    uint8_t pending_seek : 1;
    uint8_t content_length : 1;
uint8_t                       : 0;
    uint8_t failed_count;
    uint8_t writings;
  }status;
};

struct scatter_handler_result {
  std::shared_ptr<scatter_tar_file> value;
  int64_t error;
  std::wstring reason;
};
struct scheme_check_result {
  using    string = std::wstring;
  int32_t  error = 0;
  string   reason;
  string   url;
  string   content_type;
  uint64_t content_length = 0;
  bool     accept(const wchar_t* type)const;
};

struct scatter_tar_file_handler {
  using result_task = concurrency::task<scatter_handler_result>;
  using scheme_task = concurrency::task<scheme_check_result>;
  result_task async_open(std::wstring const &url);
  scheme_task async_check(std::wstring const&url);
};

