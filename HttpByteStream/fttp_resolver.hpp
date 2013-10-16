#pragma once
#include <cstdint>
#include <string>

struct fttp_uri {
  using string = std::wstring;
  string scheme;
  string full_path;
  bool validate()const;
  std::wstring true_url()const;
};
struct fttp_resolver {
  fttp_uri resolve(const wchar_t*url);
};
