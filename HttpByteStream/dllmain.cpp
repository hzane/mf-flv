#include <wrl.h>
#include <atlbase.h>
#include <strsafe.h>
#include "resource.h"
#pragma comment(lib, "runtimeobject.lib")

using namespace Microsoft::WRL;

HMODULE current_module = nullptr;
STDAPI_(BOOL) DllMain(_In_opt_ HINSTANCE hinst, DWORD reason, _In_opt_ void*) {
  if (reason == DLL_PROCESS_ATTACH) {
    current_module = hinst;
    DisableThreadLibraryCalls(hinst);
  }
  return TRUE;
}
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, _COM_Outptr_ void** ppv)
{
  return Module<InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}


STDAPI DllCanUnloadNow()
{
  return Module<InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}
STDAPI DllGetActivationFactory(_In_ HSTRING activatibleClassId, _COM_Outptr_ IActivationFactory** factory) {
  return Module<InProc>::GetModule().GetActivationFactory(activatibleClassId, factory);
}
STDAPI DllRegisterServer() {
  wchar_t modpath[MAX_PATH];
  GetModuleFileNameW(current_module, modpath, MAX_PATH);
  HRESULT hr = S_OK;
  ComPtr<IRegistrar> reg;
  hr = CoCreateInstance(CLSID_Registrar, nullptr, CLSCTX_INPROC, IID_IRegistrar, &reg);
  if (SUCCEEDED(hr))
    hr = reg->AddReplacement(L"MODULE", modpath);
  if (SUCCEEDED(hr)) hr = reg->ResourceRegister(modpath, IDR_HTTPBYTESTREAM, L"REGISTRY");

  return hr;
}

STDAPI DllUnregisterServer() {
  wchar_t modpath[MAX_PATH];
  GetModuleFileNameW(current_module, modpath, MAX_PATH);
  HRESULT hr = S_OK;
  ComPtr<IRegistrar> reg;
  hr = CoCreateInstance(CLSID_Registrar, nullptr, CLSCTX_INPROC, IID_IRegistrar, &reg);
  if (SUCCEEDED(hr)) hr = reg->AddReplacement(L"MODULE", modpath);
  if (SUCCEEDED(hr)) hr = reg->ResourceUnregister(modpath, IDR_HTTPBYTESTREAM, L"REGISTRY");

  return S_OK;
}

