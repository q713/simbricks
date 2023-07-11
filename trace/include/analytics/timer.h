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

/*
class Timer {
 protected:
  using ExecutorT = std::shared_ptr<concurrencpp::executor>;
  using ResultT = concurrencpp::result<void>;

 private:
  concurrencpp::async_lock timer_lock_;
  concurrencpp::async_condition_variable timer_cv_;

  size_t amount_waiters_ = 0;
  size_t cur_waiters_ = 0;
  std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<>> waiters_;

 public:
  explicit Timer(size_t amount_waiters) : amount_waiters_(amount_waiters) {}

  ~Timer() = default;

  ResultT
  Done(ExecutorT resume_executor) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      amount_waiters_ -= 1;
    }
    timer_cv_.notify_all();
  }

  // TODO: it may happen, that we get a deadlock as a thread is polling from the channel
  //       and he is the one currently executing regarding the timer!!!!
  //       This must be resolved to circumvent the problem
  ResultT
  MoveForward(ExecutorT resume_executor, uint64_t timestamp) {
    {
      concurrencpp::scoped_async_lock guard = co_await timer_lock_.lock(resume_executor);
      cur_waiters_++;
      waiters_.push(timestamp);

      if (cur_waiters_ == amount_waiters_ and waiters_.top() == timestamp) {
        waiters_.pop();
        cur_waiters_--;
        co_return;
      }
    }

    timer_cv_.notify_all();

    {
      auto guard = co_await timer_lock_.lock(resume_executor);
      co_await timer_cv_.await(resume_executor, guard, [this, timestamp]() {
        return cur_waiters_ == amount_waiters_ and waiters_.top() == timestamp;
      });
      cur_waiters_--;
      waiters_.pop();
    }
  }
};
*/

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
      //std::cout << "entered with: " << timestamp << std::endl;
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

      //std::cout << "waiters_that_reached_maximum: " << waiters_that_reached_maximum_ << std::endl;
      //std::cout << "current_maximum: " << cur_maximum_ << std::endl;
    }

    timer_cv_.notify_all();
  }
  // amount_waiters = 4
  // A 100 -> waiters_that_reached_maximum_ = 1, cur_max = 0, waiters = [100]
  // B 110 -> waiters_that_reached_maximum_ = 2, cur_max = 0, waiters = [100, 110]
  // C 90 -> waiters_that_reached_maximum_ = 3, cur_max = 0, waiters = [90, 100, 110]
  // D 110 -> waiters_that_reached_maximum_ = 4, cur_max = 0, waiters = [90, 100, 110A, 110C]
  //  All are notified -> C can go on which results in:
  //  waiters_that_reached_maximum_ = 3, cur_max = 90, waiters = [100, 110A, 110C]
  // C ---> awaits event from B which would arrive at 120 (the next event of C would be at 130)
  // C is stalled, as a result the timer stays in the state:
  // waiters_that_reached_maximum_ = 3, cur_max = 90, waiters = [100, 110A, 110C]
  // with no coroutine being resumed


};

class EventTimer : public Timer {
  using EventT = std::shared_ptr<Event>;

 public:
  explicit EventTimer(size_t amount_waiters) : Timer(amount_waiters) {}

  ~EventTimer() = default;

  ResultT
  MoveForward(ExecutorT resume_executor, EventT &event) {
    throw_if_empty(event, event_is_null);
    co_await Timer::MoveForward(resume_executor, event->GetTs());
    co_return;
  }
};

/*
NIC-Server try handel: NicDmaCW: source_id=2, source_name=NicbmServerParser, timestamp=446901128855552, id=5650a244af30, addr=100da4860, size=32
entered with: 1967447090000
Server-Host try handel: HostDmaW: source_id=0, source_name=Gem5ServerParser, timestamp=1967446580000, id=94904319782704, addr=100da4860, size=20
entered with: 1967446580000
Client-NIC try handel: NicMmioW: source_id=3, source_name=NicbmClientParser, timestamp=1967469341374, off=108000, len=4, val=1
entered with: 1967469341374
Client-Host try handel: HostCall: source_id=1, source_name=Gem5ClientParser, timestamp=1967446102500, pc=ffffffff81600000, func=entry_SYSCALL_64, comp=Linuxvm
entered with: 1967446102500
waiters_that_reached_maximum: 3
current_maximum: 1967446102500
...
Client-Host try handel: HostCall: source_id=1, source_name=Gem5ClientParser, timestamp=1967446738250, pc=ffffffff8148f873, func=raw_sendmsg, comp=Linuxvm
entered with: 1967446738250
waiters_that_reached_maximum: 3
current_maximum: 1967446580000
host try poll dma
--ts-lower-bound 1967446102500
1764729984999
1967446102500 Client Host
1967446580000 Server Host
1967447090000 Nic Server
1967469341374 Client NIC
*/


#endif // SIMBRICKS_TRACE_TIMER_H_
