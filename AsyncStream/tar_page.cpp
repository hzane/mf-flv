#include "stdafx.h"
#include "tar_page.hpp"
using dbg::log;
int64_t CloseTempFile(HANDLE h);  // close and delete it
HANDLE  CreateTempFile(const wchar_t* prefix);
void CALLBACK PageIoCallback(PTP_CALLBACK_INSTANCE,
                             void*,
                             void* overlapped,
                             ULONG ioresult,
                             ULONG_PTR xfered,
                             PTP_IO);

tar_page::tar_page(): _handle(CreateTempFile(L"")) {
  if (_handle != INVALID_HANDLE_VALUE) {
    tp_io = CreateThreadpoolIo(_handle, &PageIoCallback, this, nullptr);
  }
}
tar_page::~tar_page() {
  (void)close();
}

int64_t tar_page::close() {
  if (_handle == INVALID_HANDLE_VALUE)
    return 0;

  CloseThreadpoolIo(tp_io);
  tp_io = nullptr;

  auto v = CloseTempFile(_handle);
  _handle = INVALID_HANDLE_VALUE;
  return v;
}

struct overlapped_ext : OVERLAPPED{
  using event_t = concurrency::task_completion_event<int64_t>;
  event_t task_event;
  overlapped_ext(uint64_t startat);
};

overlapped_ext::overlapped_ext(uint64_t startat) {
  ZeroMemory(static_cast<LPOVERLAPPED>(this), sizeof(OVERLAPPED));
  Offset = (uint32_t)startat;
  OffsetHigh = (uint32_t)(startat >> 32);
}

read_task tar_page::async_read(uint64_t start, uint64_t size, uint8_t*buffer) {
  if (tp_io == nullptr || _handle == INVALID_HANDLE_VALUE) {  // create temp file failed
    return concurrency::task_from_result<read_result>(NTE_INVALID_HANDLE);
  }

  auto ovex = new overlapped_ext(start);
  StartThreadpoolIo(tp_io);
  auto v = ReadFile(_handle, buffer, (uint32_t)size, nullptr, ovex);
  assert(!v);  // no skip successful io
  auto t = read_task(ovex->task_event);
  auto c = GetLastError();
  if (c != ERROR_IO_PENDING){
    CancelThreadpoolIo(tp_io);
    ovex->task_event.set(c | make_sure_negative);
    delete ovex;
  }
  return t;
}

write_task tar_page::async_write(uint64_t start, uint64_t size, uint8_t const*data) {
  if (tp_io == nullptr || _handle == INVALID_HANDLE_VALUE) {  // create temp file failed
    return concurrency::task_from_result<write_result>(NTE_INVALID_HANDLE);
  }
  auto ovex = new overlapped_ext(start);
  auto t = write_task(ovex->task_event);
  StartThreadpoolIo(tp_io);
  auto v = WriteFile(_handle, data, (uint32_t)size, nullptr, ovex);
  assert(!v);  // no skip successful io
  auto c = GetLastError();

  if (c != ERROR_IO_PENDING){
    CancelThreadpoolIo(tp_io);
    ovex->task_event.set(c | make_sure_negative);
    delete ovex;
  }
  return t;
}

void CALLBACK PageIoCallback(PTP_CALLBACK_INSTANCE,
                             void* ,
                             void* overlapped,
                             ULONG ioresult,
                             ULONG_PTR xfered,
                             PTP_IO ){
  auto ov = (LPOVERLAPPED)overlapped;
  auto ovex = static_cast<overlapped_ext*>(ov);
  assert(ovex);
  ovex->task_event.set(ioresult ? (ioresult | make_sure_negative) : (uint32_t)xfered);
  delete ovex;
}

HANDLE CreateTempFile(const wchar_t* prefix){
  wchar_t tmp[MAX_PATH];
  auto l = GetTempPathW(_countof(tmp), tmp);
  if (!l)
    return INVALID_HANDLE_VALUE;

  wchar_t file_name[MAX_PATH];
  l = GetTempFileName(tmp, prefix, 0, file_name);
  if (!l)
    return INVALID_HANDLE_VALUE;
  log(L"%s\n", file_name);
  auto v = CreateFileW(file_name,
                       GENERIC_WRITE,
                       0, nullptr,
                       CREATE_ALWAYS,
                       FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS,
                       nullptr);
  return v;
}
int64_t CloseTempFile(HANDLE h) {
  if (h == INVALID_HANDLE_VALUE)
    return 0;
  wchar_t file_name[MAX_PATH];
  auto l = GetFinalPathNameByHandleW(h, file_name, _countof(file_name), FILE_NAME_NORMALIZED);
  if (!l)
    return GetLastError();
  CloseHandle(h);
  log(L"%s\n", file_name);
//  auto v = DeleteFile(file_name);
//  return v ? 0 : GetLastError();
  return 0;
}