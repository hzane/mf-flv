#include <mfapi.h>
#include "FttpSchemeHandler.hpp"
#include "TinyObject.hpp"
#include "ProgressiveHttpStream.hpp"
#include "../asyncstream/asynchttpstream/scatter_tar_file.hpp"

const CLSID CLSID_FlvByteStreamHandler = {0xEFE6208A , 0x0A2C , 0x49fa , 0x8A, 0x01 , 0x37, 0x68, 0xB5, 0x59, 0xB6, 0xDA};
const CLSID CLSID_Mp4ByteStreamHandler = {0x271C3902 , 0x6095 , 0x4c45 , 0xA2, 0x2F , 0x20, 0x09, 0x18, 0x16, 0xEE, 0x9E};

//create bytestream
//create media-source
HRESULT FttpSchemeHandler::BeginCreateObject(LPCWSTR url,
                                             DWORD flags,
                                             IPropertyStore* ,  // not used
                                             IUnknown**cancel_cookie,
                                             IMFAsyncCallback*callback,
                                             IUnknown*s) {
  if(cancel_cookie)
    *cancel_cookie = nullptr;

  if ((flags & MF_RESOLUTION_WRITE) || !(flags & MF_RESOLUTION_MEDIASOURCE))
    return E_INVALIDARG;
  if (caller_result)
    return E_FAIL;  // only one session allowed

  uri = fttp_resolver().resolve(url);
  auto hr = uri.validate() ? S_OK : E_INVALID_PROTOCOL_FORMAT;

  IMFSchemeHandlerPtr pthis(this);

  if (SUCCEEDED(hr)) hr = MFCreateAsyncResult(nullptr, callback, s, &caller_result);
  if (SUCCEEDED(hr) && uri.type == fttp_type::url)
    scatter_tar_file_handler().async_check(uri.true_url).then([this, pthis](scheme_check_result result) {
      assert(!http_stream && !source_creator);
      content_type = result.content_type;
      CopyMemory(magic, result.maigic, sizeof(magic));
      HRESULT hr = result.error; // (result.error < 0) ? (result.error | make_sure_negativei32) : S_OK;
      if (SUCCEEDED(hr) && (http_stream || source_creator)) hr = E_FAIL;
      if (SUCCEEDED(hr)) 
        hr = MakeAndInitialize<ProgressiveHttpStream, IMFByteStream>(&http_stream, result.url.c_str(), result.content_length);
      if (SUCCEEDED(hr)) hr = BeginCreateMediaSource();

      if (FAILED(hr)) {
        caller_result->SetStatus(hr);
        hr = MFInvokeCallback(caller_result.Get());
      }
    });
  else if (SUCCEEDED(hr)) {
    auto fp = uri.true_url;
    hr = MFBeginCreateFile(MF_ACCESSMODE_READ, MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, fp.c_str(), &when_create_bytestream, nullptr, nullptr);
  } 
  return hr;
}

HRESULT FttpSchemeHandler::EndCreateObject(IMFAsyncResult*result, MF_OBJECT_TYPE*type, IUnknown**obj) {
  *obj = nullptr;
  auto hr = result->GetStatus();
  if (FAILED(hr))
    return hr;
  *type = MF_OBJECT_MEDIASOURCE;
  *obj = source.Detach();

  source = nullptr;
  caller_result = nullptr;
  http_stream = nullptr;
  source_creator = nullptr;
  return S_OK;
}
HRESULT FttpSchemeHandler::CancelObjectCreation(IUnknown*) {
  return E_NOTIMPL;
}

HRESULT FttpSchemeHandler::WhenCreateMediaSource(IMFAsyncResult *result) {
  MF_OBJECT_TYPE type;
  auto hr = source_creator->EndCreateObject(result, &type, &source);
  caller_result->SetStatus(hr);
  MFInvokeCallback(caller_result.Get());
  return S_OK;
}

struct media_type {
  using string = std::wstring;
  string type;
  string subtype;
};
struct media_type_decoder {
  media_type decode(wchar_t const *ct) {
    media_type v;
    auto o = ct;
    auto r = expect_type(o, v.type);
    if (!r) r = expect_slash(o);
    if (!r) r = expect_subtype(o, v.subtype);
    return v;
  }
  int32_t expect_type(wchar_t const*&ct, std::wstring &type) {
    auto o = ct;
    while (*ct != 0 && (__iswcsym(*ct) || *ct == L'-')) ++ct;
    if (*ct == 0)
      return -1;
    type = std::wstring(o, ct);
    return 0;
  }
  int32_t expect_slash(wchar_t const*&ct) {
    if (*ct != L'/')
      return -1;
    ++ct;
    return 0;
  }
  int32_t expect_subtype(wchar_t const* &ct, std::wstring &subtype) {
    return expect_type(ct, subtype);
  }
};

/*
extension
content-type  application/octet-stream, video/mp4 audio/mp4 video/x-flv
ftyp / flv-header
*/
HRESULT determine_bytestream_handler(fttp_uri const& uri, const uint8_t *magic, size_t magic_length, const std::wstring &ct, CLSID* v) {
  magic; magic_length;
  HRESULT hr = S_OK;
  if (!ct.empty()) {
    auto mt = media_type_decoder().decode(ct.c_str());
    if (mt.type == L"video" || mt.type == L"audio") {
      if (mt.subtype == L"x-flv" || mt.subtype == L"flv") {        
        *v = CLSID_FlvByteStreamHandler;
        return hr;
      } else if (mt.subtype == L"mp4") {        
        *v = CLSID_Mp4ByteStreamHandler;
        return hr;
      }
    } else if (mt.type != L"application" || mt.subtype != L"octet-stream") {
      return E_FAIL;
    }
  }
  if (!uri.ext.empty()) {
    if (uri.ext == L".flv") {
      *v = CLSID_FlvByteStreamHandler;
      return hr;
    } else if (uri.ext == L".mp4" || uri.ext == L".f4v") {
      *v = CLSID_Mp4ByteStreamHandler;
      return hr;
    }
  }
  return hr;
}

HRESULT FttpSchemeHandler::BeginCreateMediaSource() {
  assert(!source_creator);
  CLSID bsh_clsid = CLSID_FlvByteStreamHandler;
  auto hr = determine_bytestream_handler(uri, magic, sizeof(magic), content_type, &bsh_clsid);
  if (SUCCEEDED(hr)) hr = CoCreateInstance(bsh_clsid, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMFByteStreamHandler), &source_creator);
  if (SUCCEEDED(hr))
    hr = source_creator->BeginCreateObject(http_stream.Get(), uri.true_url.c_str(), MF_RESOLUTION_MEDIASOURCE, nullptr, nullptr, &when_create_mediasource, caller_result.Get());
  return hr;
}
HRESULT FttpSchemeHandler::WhenCreateByteStream(IMFAsyncResult*result) {
  auto hr = MFEndCreateFile(result, &http_stream);
  assert(!source_creator);
  if (SUCCEEDED(hr)) hr = BeginCreateMediaSource();

  if (FAILED(hr)) {
    caller_result->SetStatus(hr);
    hr = MFInvokeCallback(caller_result.Get());
  }
  return S_OK;
}

FttpSchemeHandler::FttpSchemeHandler(): when_create_mediasource(this, &FttpSchemeHandler::WhenCreateMediaSource)
, when_create_bytestream(this, &FttpSchemeHandler::WhenCreateByteStream) {

}