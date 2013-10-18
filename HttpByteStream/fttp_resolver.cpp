#include "fttp_resolver.hpp"
#include <cpprest/uri.h>
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

bool fttp_uri::validate()const {
  return scheme == L"fttp:" && !true_url.empty() && type != fttp_type::unknown;  
}
int32_t expect_scheme(wchar_t const* &url, std::wstring &scheme) {
  auto o = url;
  while (*url != 0 && *url++ != L':') ;
  if (*url == 0)
    return -1;
  scheme = std::wstring(o, url);
  return 0;
}
//?u=[return-value]
int32_t expect_u(wchar_t const* &url, std::wstring &fullpath) {
  int32_t r = (url && *url++ == L'?') ? 0 : -1;
  if (!r) r = (*url++ == L'u') ? 0 : -1;
  if (!r) r = (*url++ == L'=') ? 0 : -1;
  if (!r) fullpath = std::wstring(url);// mf has decoded url yet
  return r;
}
#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
// ext is lowercase
int32_t guess_extension(std::wstring const&u, fttp_type type, std::wstring &ext) {
  wchar_t path[MAX_PATH] = {0, };
  if (type == fttp_type::url) {
    URL_COMPONENTS uc;
    ZeroMemory(&uc, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.dwUrlPathLength = 0 - 1ui32;

    auto i = WinHttpCrackUrl(u.c_str(), (uint32_t)u.size(), 0, &uc);
    if (i)
      wcscpy_s(path, uc.lpszUrlPath);
  }
  if (type == fttp_type::file) {
    wcscpy_s(path, u.c_str());
  }
  auto p = PathFindExtensionW(path);
  for (auto i = p; *i != 0; ++i) *i = towlower(*i);
  ext = p;
  return 0;
}
int32_t expect_type(wchar_t const* &url, fttp_type&type) {
  auto c = url;
  while (*url != 0 && *url != L'=') ++url;
  if (*url == 0)
    return -1;
  auto t = std::wstring(c, url);
  if (t == L"file?u")
    type = fttp_type::file;
  else if (t == L"url?u")
    type = fttp_type::url;
  else type = fttp_type::unknown;
  return 0;
}

//fttp://file?u=encoded_filepath
//fttp://url?u=encoded_url
fttp_uri fttp_resolver::resolve(const wchar_t*url) {
  fttp_uri v;
  auto r = expect_scheme(url, v.scheme);
  auto fakeu = std::wstring(L"http:") + std::wstring(url);

  URL_COMPONENTS uc;
  ZeroMemory(&uc, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  uc.dwExtraInfoLength = 0-1ui32;
  uc.dwSchemeLength = 0 - 1ui32;
  uc.dwHostNameLength = 0 - 1ui32;
  uc.dwUrlPathLength = 0 - 1ui32;
  uc.dwExtraInfoLength = 0 - 1ui32;

  auto i = WinHttpCrackUrl(fakeu.c_str(), (uint32_t)fakeu.size(), 0, &uc);
  if (!i)
    r = GetLastError();
  if (!r) {
    auto t = std::wstring(uc.lpszHostName, uc.dwHostNameLength);
    if (t == L"file") v.type = fttp_type::file;
    else if (t == L"url") v.type = fttp_type::url;    
  }
  url = uc.dwExtraInfoLength ? uc.lpszExtraInfo : nullptr;
  if (!r) r = expect_u(url, v.true_url);
  if (!r) r = guess_extension(v.true_url, v.type, v.ext);
  return v;
}

