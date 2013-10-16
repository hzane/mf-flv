#include "fttp_resolver.hpp"

bool fttp_uri::validate()const {
  return scheme == L"fttp:" || scheme == L"fttps:";
  // return _wcsicmp(scheme.c_str(), L"fttp:") == 0 || _wcsicmp(scheme.c_str(), L"fttps:") == 0;
}
std::wstring fttp_uri::true_url()const {
  auto ts = scheme == L"fttp:" ? L"http:" : scheme == L"fttps:" ? L"https:" : scheme;
  return ts + full_path;
}
int32_t expect_scheme(wchar_t const* &url, std::wstring &scheme) {
  auto o = url;
  while (*url != 0 && *url++ != L':') ;
  if (*url == 0)
    return -1;
  scheme = std::wstring(o, url);
  return 0;
}
int32_t expect_fullpath(wchar_t const* &url, std::wstring &fullpath) {
  fullpath = std::wstring(url);
  return 0;
}
fttp_uri fttp_resolver::resolve(const wchar_t*url) {
  fttp_uri v;
  auto r = expect_scheme(url, v.scheme);
  if (!r)
    r = expect_fullpath(url, v.full_path);
  return v;
}
