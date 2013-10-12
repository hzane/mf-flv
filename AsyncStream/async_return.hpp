#pragma once
template<typename data_t>
struct return_result{
  int64_t result;
  data_t value;
};
template<>
struct return_result<void>{
int64_t result;
};

// > 0 : bytes
// <0 : error-code
// = 0 : eos
using read_result = int64_t;
using write_result = int64_t;
