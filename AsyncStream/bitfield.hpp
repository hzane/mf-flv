#pragma once
#include <memory>
#include "ranges.hpp"

struct bitfield{
  explicit bitfield();

  uint64_t                  size = 0;  // bits
//  uint32_t alloced = 0;  // uint64's
  std::unique_ptr<uint64_t> _;

  void     mask(uint32_t idx);  // set
  bool     test(uint32_t idx)const;  //check
  void     reset(uint32_t idx);  // reset
  void     resize(uint64_t count);  //bits
  uint32_t continues(uint32_t begin, uint32_t end);  // continuous masked bits, return count
  uint32_t continues_null(uint32_t begin, uint32_t end);  // continuous unmasked bits, return count
  uint32_t first_null(uint32_t begin);
};
