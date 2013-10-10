#pragma once
#include <vector>
#include <mutex>
#include "tar_page.hpp"


using std::mutex;
using lock_guard = std::lock_guard<std::mutex>;

using tar_pages = std::vector<tar_page*>;
struct tar_file{  // random access. enable overlapped read and wwrite
	read_task  async_read(uint64_t start, uint32_t size, uint8_t* buffer);
	write_task async_write(uint64_t start, uint32_t size, uint8_t const* data);
  tar_page*  get_page(uint32_t index);
	uint64_t   reset_length(uint64_t length);
  int32_t    close();
  tar_pages  pages;

  mutex      lock;
};
