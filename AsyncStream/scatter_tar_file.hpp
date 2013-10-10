#pragma once
#include <string>
#include "tar_file.hpp"
#include "bitfield.hpp"
#include "contexts.hpp"
#include "ranges.hpp"

using download_task = concurrency::task<response_range>;

struct scatter_tar_file : public tar_file, std::enable_shared_from_this<scatter_tar_file> {
  scatter_tar_file();
  ~scatter_tar_file();
  int32_t async_open(std::wstring const&url);  //open file and begin query content-length
  //int32_t close();
  void fail_and_close(int32_t errcode);

	read_task  async_read(uint32_t size, uint8_t* buffer); // streaming read at read_pointer
	write_task async_write(uint64_t startat, uint32_t size, uint8_t const* data);
	uint64_t   reset_length(uint64_t length);
  download_task async_download_range(request_range const&rng, uint8_t*buf, uint32_t length);

  void     try_complete_read();  // if a read pending
  void     try_download_more();  // if cached data isn't enough
  void     update_write_pointer(uint64_t start, int32_t result);  // called when write complete
  void     update_read_pointer(read_result result);  // called when read complete

  uint64_t      avail_at(uint64_t startat, uint32_t expected);  // bytes
  request_range first_range_unavail_from(uint64_t start, uint32_t maxcount);  // bytes

  read_operation_context read_op_context;
  bitfield               null_slices;   // 0 : unalloced, 1 : alloced or commited block
  bitfield               commited_slices; // 0 : undownloaded, 1: downloaded block
  uint64_t               read_pointer = 0;
  uint64_t content_length = 0;
  std::wstring           url;
  struct {
    int32_t error_code;
    uint8_t reading      : 1;
    uint8_t tar_reading : 1;
    uint8_t downloadings : 4;
    uint8_t commited : 1;
    uint8_t openning : 1;
    uint8_t opened : 1;
    uint8_t closed : 1;
    uint8_t failed : 1;
    uint8_t random_access : 1;
uint8_t: 0;
    uint8_t writings ;
  }status;
  std::string error_reason;
};
