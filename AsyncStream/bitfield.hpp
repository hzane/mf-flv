#pragma once
#include <memory>
#include "ranges.hpp"

struct bitfield{
  explicit bitfield();

  uint64_t                  size = 0;  // bits
  std::unique_ptr<uint64_t> _;

  void     mask(uint64_t idx);  // set
  bool     test(uint64_t idx)const;  //check
  void     reset(uint64_t idx);  // reset
  void     resize(uint64_t count);  //bits
  uint64_t continues(uint64_t begin, uint64_t end);  // continuous masked bits, return count
  uint64_t continues_null(uint64_t begin, uint64_t end);  // continuous unmasked bits, return count
  uint64_t first_null(uint64_t begin);
};
