#pragma once
#include <wrl\implements.h>
#include "AsyncCallback.hpp"

using namespace Microsoft::WRL;

template<typename state_t>
class TinyObject : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IUnknown> {
public:
  state_t state;
  TinyObject() = default;
  HRESULT RuntimeClassInitialize(state_t const&v)
  {
    state = v;
    return S_OK;
  }
};

template<typename state_t>
state_t&
FromI(IUnknown*u){
  auto pthis = static_cast<TinyObject<state_t>*>(u);
  return pthis->state;
}

template<typename state_t>
state_t&
FromAsyncResultObject(IMFAsyncResult*result){
  ComPtr<IUnknown> obj;
  result->GetObject(&obj);
  return FromI<state_t>(obj.Get());
}

template<typename state_t>
state_t&
FromAsyncResultState(IMFAsyncResult*result){
  ComPtr<IUnknown> obj;
  result->GetState(&obj);
  return FromI<state_t>(obj.Get());
}

typedef ComPtr<IUnknown> TinyObjectPtr;
template<typename state_t>
TinyObjectPtr
NewTinyObject(state_t const&val){
  MFStatePtr obj;
  auto hr = MakeAndInitialize<TinyObject<state_t>>(&obj, val);  // ignore hresult
  hr;
  return obj;
}
