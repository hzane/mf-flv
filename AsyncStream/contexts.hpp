#pragma once

using read_task_completion_event = concurrency::task_completion_event<read_result>;

using task_completion_event_ptr = std::shared_ptr<read_task_completion_event>;
using status_code = web::http::status_code;
using body_stream = concurrency::streams::istream; // body's type
using http_client = web::http::client::http_client;
using http_request = web::http::http_request;
using http_response = web::http::http_response;
using response_task = concurrency::task<http_response>;
using body_read_task = concurrency::task<size_t>;
using binary_task = concurrency::task<int64_t>;
using binary_buff = concurrency::streams::rawptr_buffer<uint8_t>;
using byte_array = std::shared_ptr<uint8_t>;
using http_headers = web::http::http_headers;
using http_method = web::http::method;
using http_methods = web::http::methods;
using status_codes = web::http::status_codes;
using concurrency::task_from_result;
using concurrency::task_from_exception;

struct read_operation_context{
  task_completion_event_ptr read_task_event;
  uint64_t expected;
  uint8_t* buffer;
  uint64_t start_position;
  void reset();
};
struct http_response_noex {
  http_response _;
  std::string   reason;
  int64_t       code = 0;
  http_response_noex() = default;
  ~http_response_noex() = default;
  explicit http_response_noex(const char*what, int64_t c):code(c), reason(what) {};
  explicit http_response_noex(http_response &&resp):_(resp) {};
};
using resp_task = concurrency::task<http_response_noex>;
