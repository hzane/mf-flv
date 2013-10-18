#pragma once
#include <cstdint>
#include "contexts.hpp"

using read_task  = concurrency::task<read_result>;
using write_task = concurrency::task<write_result>;

// not use any locks, but there is a risk for _handle may be invalid when closing
struct tar_page{
  read_task  async_read(uint64_t start, uint64_t size, uint8_t* buffer);
  write_task async_write(uint64_t start, uint64_t size, uint8_t const* data);
  int64_t    close();

  HANDLE _handle = INVALID_HANDLE_VALUE;
  PTP_IO tp_io   = nullptr;
  tar_page();  // will create a temporary file
  ~tar_page(); // will close temporary file and delete it
};
