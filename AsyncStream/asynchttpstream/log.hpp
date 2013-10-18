#pragma once
namespace dbg{
// use outputdebugstring to dump dbg infos
	void log(char const* fmt, ...);
	void log(wchar_t const* fmt, ...);
}