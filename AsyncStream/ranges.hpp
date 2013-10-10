#pragma once
#include <cstdint>
#include <string>
#include "constants.hpp"

using std::wstring;
struct request_range{
  uint64_t head = 0 - 1ull;
  uint64_t tail = 0;

  uint64_t size()const;
  bool     empty()const;
  int32_t  error_code()const;
  request_range() = default;
  request_range operator*(uint32_t scale)const;
  request_range operator/(uint32_t scale)const;
  request_range operator-(request_range const&)const;

  explicit request_range(uint64_t h, uint64_t t);
  wstring to_string()const;  // bytes=head-tail
};

struct response_range : request_range{
  uint64_t instance_size;
  explicit response_range(uint64_t h, uint64_t t, uint64_t insl);
  response_range() = default;
  response_range(const response_range&) = default;
};

struct response_range_decoder {
  response_range decode(wstring const&s);
};

inline request_range error_request_range(int32_t errcode){
  auto v = uint64_t(errcode | make_sure_negative);
  return request_range{v, v -1};
}
inline response_range error_response_range(int32_t errcode){
  auto v = uint64_t(errcode | make_sure_negative);
  return response_range{v, v-1, 0};
}

inline response_range::response_range(uint64_t h, uint64_t t, uint64_t insl) : request_range(h, t), instance_size(insl) {

}

inline int32_t request_range::error_code()const {
  if (head <= tail)
    return 0;
  return int32_t(head);
}

inline request_range request_range::operator*(uint32_t scale)const {
  auto v = *this;
  v.head *= scale;
  v.tail = (v.tail + 1) * scale - 1;
  return v;
}
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