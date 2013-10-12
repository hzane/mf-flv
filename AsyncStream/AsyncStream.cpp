// console.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "cmdline.h"
#include "tarstream.hpp"
#include <cpprest\asyncrt_utils.h>

int main_imp(gengetopt_args_info& ai)
{
  auto st = std::make_shared<scatter_tar_file>();
  st->async_head(utility::conversions::to_string_t(ai.url_arg));
  //uint8_t buf[128];
 // st->async_read(128, buf);
  getchar();
  st->fail_and_close(0);
  return 0;
}
int main(int argc, char* argv[])
{
  gengetopt_args_info args_info;
  /* let's call our cmdline parser */
  if (cmdline_parser(argc, argv, &args_info) != 0)
    return 1;
  auto r = main_imp(args_info);
  cmdline_parser_free(&args_info); /* release allocated memory */
	return r;
}

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