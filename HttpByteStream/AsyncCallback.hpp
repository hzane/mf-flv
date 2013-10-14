#pragma once
#include <mfidl.h>

template<class Parent>// T: Type of the parent object
class AsyncCallback: public IMFAsyncCallback {
  typedef HRESULT(STDMETHODCALLTYPE Parent::*InvokeFn)(IMFAsyncResult *);
  Parent    *parent = nullptr;
  InvokeFn  invoke = nullptr;
public:
  AsyncCallback(Parent *pParent, InvokeFn fn): parent(pParent), invoke(fn) {}

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
  STDMETHODIMP_(ULONG) AddRef() { return parent->AddRef(); }
  STDMETHODIMP_(ULONG) Release() { return parent->Release(); }

  // IMFAsyncCallback methods
  STDMETHODIMP GetParameters(DWORD*, DWORD*) { return E_NOTIMPL; }
  STDMETHODIMP Invoke(IMFAsyncResult* pAsyncResult) { return (parent->*invoke)(pAsyncResult); }
};

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
template<typename Parent>
STDMETHODIMP AsyncCallback<Parent>::QueryInterface(REFIID riid, void** ppv) {
  static const QITAB qit[] =
  {
    QITABENT(AsyncCallback, IMFAsyncCallback),
    {0}
  };
  return QISearch(this, qit, riid, ppv);
}