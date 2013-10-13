#pragma once

#include "targetver.h"
#include <Windows.h>
#include <cstdint>
#include <ppltasks.h>
#include <cpprest/http_client.h>
#include <cpprest/streams.h>
#include <cpprest/rawptrstream.h>

#include "constants.hpp"

namespace dbg {
  void log(const char*fmt, ...);
  void log(const wchar_t*fmt, ...);
}