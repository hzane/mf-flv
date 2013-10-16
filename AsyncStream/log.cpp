#include "stdafx.h"
#include "asynchttpstream/log.hpp"
void dbg::log(const wchar_t*fmt, ...) {
  wchar_t dst[16384];
  va_list ap;
  va_start(ap, fmt);
  vwprintf_s(fmt, ap);
  auto r = vswprintf_s(dst, fmt, ap);
  va_end(ap);
  if (r > 0)
    OutputDebugStringW(dst);
}
void dbg::log(const char*fmt, ...) {
  char dst[16384];
  va_list ap;
  va_start(ap, fmt);
  auto r = vsprintf_s(dst, fmt, ap);
  vprintf_s(fmt, ap);
  va_end(ap);
  if (r > 0)
    OutputDebugStringA(dst);
}