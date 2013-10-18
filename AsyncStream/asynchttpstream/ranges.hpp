#pragma once
#include <cstdint>
#include <string>
#include "constants.hpp"

using wstring = std::wstring;
struct request_range{
  uint64_t head = 0 - 1ui64;
  uint64_t tail = 0;

  uint64_t size()const;
  bool     empty()const;
  int64_t  error_code()const;

  request_range() = default;

  request_range operator*(uint32_t scale)const;
  request_range operator/(uint32_t scale)const;
  request_range operator-(request_range const&)const;

  explicit request_range(uint64_t h, uint64_t t);
  wstring to_string()const;  // bytes=head-tail
};

struct response_range : request_range{
  uint64_t instance_size = 0;

  response_range() = default;
  response_range(const response_range&) = default;
  explicit response_range(uint64_t head, uint64_t tail, uint64_t instlength);
};

// parse http header content-range
// like 'bytes head-tail/instance_size'
struct response_range_decoder {
  response_range decode(wstring const&s);
};

// wrap error code into struct request_range
// used for return errcode
// make sure head high bit is 1 and head > tail
inline request_range error_request_range(int64_t errcode) {
  auto v = uint64_t(errcode | make_sure_negative | make_sure_negativei32);
  return request_range{v, v -1};
}

inline response_range error_response_range(int64_t errcode) {
  auto v = uint64_t(errcode | make_sure_negative | make_sure_negativei32);
  return response_range{v, v-1, 0};
}

inline response_range::response_range(uint64_t head, uint64_t tail, uint64_t instlength)
    : request_range(head, tail), instance_size(instlength) {

}

inline int64_t request_range::error_code()const {
  if (head <= tail || !(head & make_sure_negative))
    return 0;
  return int64_t(head);
}


// convert [head, tail] to [head  * scale, (tail+1) * scale - 1]
// the same means : [head, tail +1) to = [head * scale, (tail +1) * scale)
inline request_range request_range::operator*(uint32_t scale)const {
  auto v = *this;
  v.head *= scale;
  v.tail = (v.tail + 1) * scale - 1;
  return v;
}

// convert bytes-range to slice-range
// [head, tail] to [head / scale, (tail + scale) / scale)
inline request_range request_range::operator/(uint32_t scale)const {
  auto v = *this;
  v.head = v.head / scale;
  v.tail = (v.tail + scale) / scale - 1;
  return v;
}


inline request_range request_range::operator-(request_range const&rhs)const {
  assert(head == rhs.head);
  auto v = *this;
  v.head = rhs.tail + 1;
  v.tail = tail;
  return v;
}