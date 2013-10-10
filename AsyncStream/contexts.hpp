#pragma once

using read_task_completion_event = concurrency::task_completion_event<read_result>;

using task_completion_event_ptr = std::shared_ptr<read_task_completion_event>;
struct read_operation_context{
  task_completion_event_ptr read_task_event;
  uint32_t expected;
  uint8_t* buffer;
  uint64_t start_position;
  void reset();
};
