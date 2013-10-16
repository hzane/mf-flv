#include "stdafx.h"
#include "asynchttpstream/ranges.hpp"

request_range::request_range(uint64_t h, uint64_t t): head(h), tail(t){}

uint64_t request_range::size()const{
  if (head > tail)
    return 0;
  return tail +1 - head;
}

bool request_range::empty()const{
  return head > tail;
}
wstring request_range::to_string()const {
  wchar_t buf[128];
  (void)swprintf_s(buf, L"bytes=%d-%I64u", head, tail);
  return buf;
}

response_range error_response_range(int64_t code);
// bytes [sp] head-tail/instance_length
// bytes [sp] */*
response_range response_range_decoder::decode(std::wstring const&s) {
  auto sppos = s.find_first_of(L' ');
  auto bu = s.substr(0, sppos);  // bytes
  auto sp2 = s.find_first_of(L'/', sppos); // '/'
  auto rang = s.substr(sppos + 1, sp2 - sppos - 1);  // head-tail | *
  auto il = s.substr(sp2 + 1);  // instancelength | *
  auto sp3 = rang.find(L'-');
  if (sp3 == wstring::npos) {
    return error_response_range(e_tar_stream_range);
  }
  auto h = rang.substr(0, sp3);
  auto t = rang.substr(sp3 + 1);

  response_range v;
  v.head = _wtoi64(h.c_str());
  v.tail = _wtoi64(t.c_str());

  v.instance_size = _wtoi64(il.c_str());
  return v;
}
