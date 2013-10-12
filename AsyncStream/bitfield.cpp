#include "stdafx.h"
#include "bitfield.hpp"
#include <algorithm>

bitfield::bitfield() {
}
void bitfield::resize(uint64_t count){
  // what if count is zero
  auto nalloc = (count + bits_container_size - 1) / bits_container_size;
  auto oalloc = (size + bits_container_size - 1) / bits_container_size;
  if (oalloc == nalloc) {
    return;
  }
  auto n = new uint64_t[(uint64_t)nalloc];
  memset(n, 0, (uint64_t)nalloc * byte_per_ui64);
  memcpy(n, _.get(), (uint64_t)min(oalloc, nalloc));
  _.reset(n);
  size = count;
}

bool bitfield::test(uint64_t idx)const {
  if (idx >= size)
    return false;
  auto i = idx / bits_container_size;
  auto offset = idx % bits_container_size;
  auto x = _.get()[i];
  return (x & (1ui64 << offset)) != 0;
}

void bitfield::mask(uint64_t idx) {
  if (idx >= size)  {
    resize(idx + bits_container_size);
  }
  auto i = idx / bits_container_size;
  auto offset = idx % bits_container_size;
  _.get()[i] |= 1ui64 << offset;
}

void bitfield::reset(uint64_t idx) {
  if (idx >= size)
    return;
  auto i = idx / bits_container_size;
  auto offset = idx % bits_container_size;
  _.get()[i] &= ~(1ui64 << offset);
}

uint64_t bitfield::continues(uint64_t begin, uint64_t end) {
  uint64_t v = 0;
  while (begin < end && test(begin++)) ++v;
  return v;
}
uint64_t bitfield::continues_null(uint64_t begin, uint64_t end) {
  uint64_t v = 0;
  while (begin < end && !test(begin++)) ++v;
  return v;
}

uint64_t bitfield::first_null(uint64_t begin) {
  auto end = (size + bits_container_size - 1) / bits_container_size;
  while (begin < end && test(begin)) ++begin;
  return begin;
}