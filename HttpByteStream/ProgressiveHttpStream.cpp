#include <cstdio>
#include <cassert>
#include <mfapi.h>
#include "../AsyncStream/asynchttpstream/contexts.hpp"
#include "ProgressiveHttpStream.hpp"
#include "../AsyncStream/asynchttpstream/log.hpp"

#pragma comment(lib, "mfplat.lib")

using callback_result = ComPtr<IMFAsyncResult>;
using dbg::log;

HRESULT ProgressiveHttpStream::BeginRead(BYTE *pb, ULONG cb, IMFAsyncCallback*call, IUnknown*s){
  callback_result caller_result;
  auto hr = MFCreateAsyncResult(nullptr, call, s, &caller_result);
  if(SUCCEEDED(hr)) _impl->async_read(cb, pb).then([caller_result](read_result readed) {
    caller_result->SetStatus(readed < 0 ? uint32_t(readed) | make_sure_negativei32 : (uint32_t)readed);
    MFInvokeCallback(caller_result.Get());
  });
  return hr;
}
HRESULT ProgressiveHttpStream::EndRead(IMFAsyncResult*result, ULONG *readed) {
  auto hr = result->GetStatus();
  if (SUCCEEDED(hr))
    *readed = hr;
  return hr > 0 ? S_OK : hr;
}
HRESULT ProgressiveHttpStream::GetLength(QWORD *length) {
  lock_guard g(_impl->lock);
  *length = _impl->content_length;
  log("http-stream content-length %I64u\n", *length);
  return S_OK;
}
HRESULT ProgressiveHttpStream::GetCurrentPosition(QWORD *pos) {
  lock_guard g(_impl->lock);
  *pos = _impl->read_pointer;
  return S_OK;
}

HRESULT ProgressiveHttpStream::SetCurrentPosition(QWORD pos) {
  lock_guard g(_impl->lock);
  (void)_impl->seek(pos);
  log("http-stream set-current-pos %I64u\n", pos);
  return S_OK;
}

HRESULT ProgressiveHttpStream::IsEndOfStream(BOOL *eos) {
  lock_guard g(_impl->lock);
  if (_impl->read_pointer >= _impl->content_length)
    *eos = TRUE;
  log("http-stream is-end-of-stream %i\n", *eos);
  return S_OK;
}

HRESULT ProgressiveHttpStream::Seek(MFBYTESTREAM_SEEK_ORIGIN origin, LONGLONG seek_offset, DWORD , QWORD *current){
  lock_guard g(_impl->lock);
  auto true_read_pointer = _impl->status.pending_seek ? _impl->status.seek_pointer : _impl->read_pointer;
  if(origin == MFBYTESTREAM_SEEK_ORIGIN::msoBegin)
    *current = seek_offset;
  else if(origin == MFBYTESTREAM_SEEK_ORIGIN::msoCurrent)
    *current = true_read_pointer + seek_offset;
  else *current = true_read_pointer;
  *current = _impl->seek(*current);
  return S_OK;
}

HRESULT ProgressiveHttpStream::RuntimeClassInitialize(wchar_t const*url, UINT64 contentlength) {
  _impl = make_shared<scatter_tar_file>();
  return (HRESULT)_impl->async_open(url, contentlength);
}

HRESULT ProgressiveHttpStream::Close() {
  _impl->fail_and_close(0);
  log("http-stream close\n");
  return S_OK;
}
HRESULT ProgressiveHttpStream::GetCapabilities(DWORD *capa) {
  if (capa == nullptr)
    return E_INVALIDARG;

  *capa = MFBYTESTREAM_IS_READABLE
    | MFBYTESTREAM_IS_REMOTE | MFBYTESTREAM_IS_SEEKABLE
      | MFBYTESTREAM_IS_PARTIALLY_DOWNLOADED
      | MFBYTESTREAM_HAS_SLOW_SEEK;
  log("http-stream get-capa %#X\n", *capa);
  return S_OK;
}

HRESULT ProgressiveHttpStream::Read(BYTE *pb, ULONG cb, ULONG *readed) {
  auto r = _impl->async_read(cb, pb).get();
  HRESULT hr = S_OK;
  if (r >= 0)
    *readed = (uint32_t)r;
  else hr = (HRESULT)r | make_sure_negativei32;
  return hr;
}

HRESULT ProgressiveHttpStream::SetLength( QWORD ) {
  log("http-stream set-length invalid");
  return E_NOTIMPL;
}
HRESULT ProgressiveHttpStream::Write(const BYTE *, ULONG, ULONG *){
  log("http-stream write invalid\n");
  return E_NOTIMPL;
}

HRESULT ProgressiveHttpStream::BeginWrite(const BYTE *,  ULONG ,  IMFAsyncCallback *,  IUnknown * ){
  log("http-stream begin-write invalid\n");
  return E_NOTIMPL;
}

HRESULT ProgressiveHttpStream::EndWrite( IMFAsyncResult *, ULONG *){
  log("http-stream end-write invalid\n");
  return E_NOTIMPL;
}

HRESULT ProgressiveHttpStream::Flush(void){
  return S_OK;
}

