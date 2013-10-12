#include "stdafx.h"
#include "tar_file.hpp"

read_task tar_file::async_read(uint64_t start, uint64_t size, uint8_t*buffer){
	auto idx = start / tar_page_size;
	auto in_page_offset = start % tar_page_size;
	auto page = get_page((uint32_t)idx);
	return page->async_read(in_page_offset, size, buffer);
}

//find page and write to page
write_task tar_file::async_write(uint64_t start, uint64_t size, uint8_t const*data){
	auto idx = start / tar_page_size;
	auto in_page_offset = start % tar_page_size;
	auto page = get_page((uint32_t)idx);
  return page->async_write(in_page_offset, size, data);
}

//intialize page and return it
tar_page* tar_file::get_page(uint64_t idx) {
  lock_guard gd(lock);
  if (pages.size() <= idx)
    pages.resize(idx +1);
  auto v = pages[idx];
  if (v == nullptr){
    v = new tar_page();
    pages[idx] = v;
  }
  return v;
}

int64_t tar_file::close() {
  lock_guard gd(lock);
  for(auto &x : pages){
    if (!x)
      continue;
    x->close();
    delete x;
  }
  return 0;
}
