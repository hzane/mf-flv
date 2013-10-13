#include <wrl.h>
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
