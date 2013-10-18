#include "stdafx.h"
#include "cmdline.h"
#include "tarstream.hpp"
#include <cpprest\asyncrt_utils.h>

int main_imp(gengetopt_args_info& ai)
{
  using utility::conversions::to_string_t;
  auto r = scatter_tar_file_handler().async_open(to_string_t(ai.url_arg));
  auto v = r.get();
  getchar();
  if (v.value)
    v.value->fail_and_close(0);
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