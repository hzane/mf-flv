#pragma once
#include <wrl.h>
#include <mfidl.h>
#include "AsyncCallback.hpp"
#include "fttp_resolver.hpp"

using namespace Microsoft::WRL;
using IMFByteStreamHandlerPtr = ComPtr<IMFByteStreamHandler>;
using IMFMediaSourcePtr       = ComPtr<IMFMediaSource>;
using IMFAsyncResultPtr       = ComPtr<IMFAsyncResult>;
using IMFByteStreamPtr        = ComPtr<IMFByteStream>;
using IMFSchemeHandlerPtr     = ComPtr<IMFSchemeHandler>;

//{CD27D61F-1E98-46B6-B239-53D0C630EA06}
struct __declspec(uuid("CD27D61F-1E98-46B6-B239-53D0C630EA06"))
FttpSchemeHandler : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFSchemeHandler>{
  STDMETHODIMP BeginCreateObject(LPCWSTR url,
                                 DWORD flags,
                                 IPropertyStore *props,
                                 IUnknown **cancel,
                                 IMFAsyncCallback *cb,
                                 IUnknown *s);

  STDMETHODIMP EndCreateObject(IMFAsyncResult *result, MF_OBJECT_TYPE *objtype,IUnknown **obj);
  STDMETHODIMP CancelObjectCreation(IUnknown *cancel_cookie);

  FttpSchemeHandler();
  HRESULT RuntimeClassInitialize() { return S_OK; };
protected:
  using Callback = AsyncCallback<FttpSchemeHandler>;
  IMFByteStreamHandlerPtr flv_source_creator;
  IMFMediaSourcePtr       flv_source;
  IMFAsyncResultPtr       caller_result;
  IMFByteStreamPtr        http_stream;
  Callback                when_bsh_create_object;
  fttp_uri                uri;
  STDMETHODIMP WhenBshCreateObject(IMFAsyncResult*);
};

CoCreatableClass(FttpSchemeHandler);