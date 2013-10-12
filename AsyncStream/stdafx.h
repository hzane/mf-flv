// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"
#include <Windows.h>
#include <cstdint>
#include <ppltasks.h>
#include "constants.hpp"
//#include <cpprest/filestream.h>

namespace dbg {
  void log(const char*fmt, ...);
  void log(const wchar_t*fmt, ...);
}