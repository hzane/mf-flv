#pragma once
#include <wrl.h>
#include <mfidl.h>
#include "AsyncCallback.hpp"
#include "../AsyncStream/asynchttpstream/scatter_tar_file.hpp"
using namespace Microsoft::WRL;
struct ProgressiveHttpStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFByteStream>{
  STDMETHODIMP GetCapabilities(DWORD *pdwCapabilities);
  STDMETHODIMP GetLength(QWORD *pqwLength);
  STDMETHODIMP SetLength(QWORD qwLength) ;
  STDMETHODIMP GetCurrentPosition(QWORD *pqwPosition);
  STDMETHODIMP SetCurrentPosition(QWORD qwPosition);
  STDMETHODIMP IsEndOfStream(BOOL *pfEndOfStream);
  STDMETHODIMP Read(BYTE *pb, ULONG cb, ULONG *pcbRead);
  STDMETHODIMP BeginRead(BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState);
  STDMETHODIMP EndRead(IMFAsyncResult *pResult, ULONG *pcbRead);
  STDMETHODIMP Write(const BYTE *pb, ULONG cb, ULONG *pcbWritten);
  STDMETHODIMP BeginWrite(const BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState);
  STDMETHODIMP EndWrite(IMFAsyncResult *pResult, ULONG *pcbWritten);
  STDMETHODIMP Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD dwSeekFlags, QWORD *pqwCurrentPosition);
  STDMETHODIMP Flush(void);
  STDMETHODIMP Close(void);

public:
  ProgressiveHttpStream() = default;
  HRESULT RuntimeClassInitialize(wchar_t const*url, UINT64 contentlength);
protected:
  ~ProgressiveHttpStream() = default;
  using callback_stub = AsyncCallback<ProgressiveHttpStream>;

  std::shared_ptr<scatter_tar_file> _impl;
};
