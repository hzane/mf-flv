// console.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "cmdline.h"

int main_imp(gengetopt_args_info& ai)
{
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

