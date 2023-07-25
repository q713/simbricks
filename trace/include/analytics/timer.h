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

#include <memory>
#include <queue>
#include <functional>

#include "corobelt/corobelt.h"
#include "events/events.h"

#ifndef SIMBRICKS_TRACE_TIMER_H_
#define SIMBRICKS_TRACE_TIMER_H_

class Timer {
 protected:
  using ExecutorT = std::shared_ptr<concurrencpp::executor>;
  using ResultT = concurrencpp::result<void>;

 private:
  concurrencpp::async_lock timer_lock_;
  concurrencpp::async_condition_variable timer_cv_;
  size_t amount_waiters_ = 0;
  uint64_t cur_maximum_ = 0;
  size_t waiters_that_reached_maximum_ = 0;
  std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<>> waiters_;

 public:
  explicit Timer(size_t amount_waiters) : amount_waiters_(amount_waiters) {}

  ResultT Done(ExecutorT resume_executor) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      --amount_waiters_;
    }

    timer_cv_.notify_all();
  }

  ResultT MoveForward(ExecutorT resume_executor, uint64_t timestamp) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      if (timestamp <= cur_maximum_) {
        co_return;
      }

      ++waiters_that_reached_maximum_;
      waiters_.push(timestamp);
    }

    timer_cv_.notify_all();

    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      co_await timer_cv_.await(resume_executor, guard, [this, timestamp]() {
        return (waiters_that_reached_maximum_ == amount_waiters_ and waiters_.top() == timestamp)
               or (cur_maximum_ >= timestamp);
      });
      if (timestamp > cur_maximum_) {
        cur_maximum_ = timestamp;
      }
      --waiters_that_reached_maximum_;
      waiters_.pop();
    }

    timer_cv_.notify_all();
  }
};

#endif // SIMBRICKS_TRACE_TIMER_H_
