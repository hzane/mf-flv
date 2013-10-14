#pragma once
#include <wrl.h>
#include <mfidl.h>
#include "AsyncCallback.hpp"
using namespace Microsoft::WRL;
struct ProgressiveHttpStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMFByteStream>{
  STDMETHODIMP GetCapabilities(DWORD *pdwCapabilities) { return 0; };
  STDMETHODIMP GetLength(QWORD *pqwLength) { return 0; };
  STDMETHODIMP SetLength(QWORD qwLength) { return 0; };
  STDMETHODIMP GetCurrentPosition(QWORD *pqwPosition) { return 0; };
  STDMETHODIMP SetCurrentPosition(QWORD qwPosition) { return 0; };
  STDMETHODIMP IsEndOfStream(BOOL *pfEndOfStream) { return 0; };
  STDMETHODIMP Read(BYTE *pb, ULONG cb, ULONG *pcbRead) { return 0; };
  STDMETHODIMP BeginRead(BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState) { return 0; };
  STDMETHODIMP EndRead(IMFAsyncResult *pResult, ULONG *pcbRead) { return 0; };
  STDMETHODIMP Write(const BYTE *pb, ULONG cb, ULONG *pcbWritten) { return 0; };
  STDMETHODIMP BeginWrite(const BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState) { return 0; };
  STDMETHODIMP EndWrite(IMFAsyncResult *pResult, ULONG *pcbWritten) { return 0; };
  STDMETHODIMP Seek(MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD dwSeekFlags, QWORD *pqwCurrentPosition) { return 0; };
  STDMETHODIMP Flush(void) { return 0; };
  STDMETHODIMP Close(void) { return 0; };

public:
  ProgressiveHttpStream();
protected:
  ~ProgressiveHttpStream() = default;
  AsyncCallback<ProgressiveHttpStream> on_what;
};

