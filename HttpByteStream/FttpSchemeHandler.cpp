#include <mfapi.h>
#include "FttpSchemeHandler.hpp"
#include "TinyObject.hpp"
#include "ProgressiveHttpStream.hpp"
#include "../asyncstream/asynchttpstream/scatter_tar_file.hpp"

const CLSID CLSID_FlvSourceHandler = {0xEFE6208A , 0x0A2C , 0x49fa , 0x8A, 0x01 , 0x37, 0x68, 0xB5, 0x59, 0xB6, 0xDA};
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
  if (SUCCEEDED(hr))
    scatter_tar_file_handler().async_check(uri.true_url()).then([this, pthis](scheme_check_result result) {
      HRESULT hr = (result.error) ? (result.error | make_sure_negativei32) : S_OK;
      if (SUCCEEDED(hr)) hr = result.accept(L"flv") ? S_OK : E_SUBPROTOCOL_NOT_SUPPORTED;
      if (SUCCEEDED(hr) && (http_stream || flv_source_creator)) hr = E_FAIL;
      if (SUCCEEDED(hr)) 
        hr = MakeAndInitialize<ProgressiveHttpStream, IMFByteStream>(&http_stream, result.url.c_str(), result.content_length);
      if (SUCCEEDED(hr)) 
        hr = CoCreateInstance(CLSID_FlvSourceHandler, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IMFByteStreamHandler), &flv_source_creator);
      if (SUCCEEDED(hr))
        hr = flv_source_creator->BeginCreateObject(http_stream.Get(), result.url.c_str(), MF_RESOLUTION_MEDIASOURCE, nullptr, nullptr, &when_bsh_create_object, caller_result.Get());

      if (FAILED(hr)) {
        caller_result->SetStatus(hr);
        hr = MFInvokeCallback(caller_result.Get());
      }
    });
  return hr;
}

HRESULT FttpSchemeHandler::EndCreateObject(IMFAsyncResult*result, MF_OBJECT_TYPE*type, IUnknown**obj) {
  *obj = nullptr;
  auto hr = result->GetStatus();
  if (FAILED(hr))
    return hr;
  *type = MF_OBJECT_MEDIASOURCE;
  *obj = flv_source.Detach();

  flv_source = nullptr;
  caller_result = nullptr;
  http_stream = nullptr;
  flv_source_creator = nullptr;
  return S_OK;
}
HRESULT FttpSchemeHandler::CancelObjectCreation(IUnknown*) {
  return E_NOTIMPL;
}

HRESULT FttpSchemeHandler::WhenBshCreateObject(IMFAsyncResult *result) {
  MF_OBJECT_TYPE type;
  auto hr = flv_source_creator->EndCreateObject(result, &type, &flv_source);
  caller_result->SetStatus(hr);
  MFInvokeCallback(caller_result.Get());
  return S_OK;
}

FttpSchemeHandler::FttpSchemeHandler() : when_bsh_create_object(this, &FttpSchemeHandler::WhenBshCreateObject) {

}