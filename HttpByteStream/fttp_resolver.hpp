#pragma once
#include <cstdint>
#include <string>

enum class fttp_type : uint32_t {
  unknown, url, file
};
enum class fttp_media_type: uint32_t {
  unknown, flv, mp4
};
//fttp://file?u=encoded_filepath
//fttp://url?u=encoded_url
struct fttp_uri {
  using           string     = std::wstring;
  string          scheme;
  string          true_url;
  string          ext;  // .mp4 or .flv etc
  fttp_type       type       = fttp_type::unknown;
  bool validate()const;
};
struct fttp_resolver {
  fttp_uri resolve(const wchar_t*url);
};
