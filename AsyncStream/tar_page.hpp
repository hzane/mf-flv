#pragma once
#include <cstdint>
#include <ppltasks.h>
#include "async_return.hpp"

using read_task  = concurrency::task<read_result>;
using write_task = concurrency::task<write_result>;
struct tar_page{
  read_task  async_read(uint64_t start, uint32_t size, uint8_t* buffer);
  write_task async_write(uint64_t start, uint32_t size, uint8_t const* data);
  int32_t    close();

  HANDLE _handle = INVALID_HANDLE_VALUE;
  PTP_IO tp_io   = nullptr;
  tar_page();  // will create a temporary file
  ~tar_page();  // will close temporary file and delete it
};
