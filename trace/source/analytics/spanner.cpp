/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "analytics/spanner.h"

concurrencpp::result<void> Spanner::consume(
    ExecutorT resume_executor, std::shared_ptr<Channel<EventT>> &src_chan) {
  throw_if_empty(resume_executor, resume_executor_null);
  throw_if_empty(src_chan, channel_is_null);

  std::shared_ptr<Event> event_ptr = nullptr;
  bool added = false;
  std::optional<std::shared_ptr<Event>> event_ptr_opt;

  //size_t timer_key = co_await timer_.Register(resume_executor);

  for (event_ptr_opt = co_await src_chan->Pop(resume_executor); event_ptr_opt.has_value();
       event_ptr_opt = co_await src_chan->Pop(resume_executor)) {

    event_ptr = event_ptr_opt.value();
    throw_if_empty(event_ptr, event_is_null);

    // co_await timer_.MoveForward(resume_executor, event_ptr->GetTs());
    // co_await timer_.MoveForward(resume_executor, timer_key, event_ptr->GetTs());
    std::cout << name_ << " try handel: " << *event_ptr << std::endl;

    auto handler_it = handler_.find(event_ptr->GetType());
    if (handler_it == handler_.end()) {
      std::cerr << "Spanner: could not find handler for the following event: ";
      if (event_ptr) {
        std::cerr << *event_ptr;
      } else {
        std::cerr << "null";
      }
      std::cerr << std::endl;
      continue;
    }

    auto handler = handler_it->second;
    added = co_await handler(resume_executor, event_ptr);
    if (not added) {
      std::cout << "found event that could not be added to a pack: "
                << *event_ptr << std::endl;
    }
  }

  //co_await timer_.Done(resume_executor);
  //co_await timer_.Done(resume_executor, timer_key);

  std::cout << "event spanner exited" << std::endl;

  co_return;
}
