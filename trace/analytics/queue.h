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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
#define SIMBRICKS_TRACE_CONTEXT_QUEUE_H_

#include <concurrencpp/executors/executor.h>
#include <concurrencpp/results/result.h>
#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>

#include "span.h"
#include "exception.h"
#include "corobelt.h"

enum expectation { tx, rx, dma, msix, mmio };

inline std::ostream &operator<<(std::ostream &os, expectation e) {
  switch (e) {
    case expectation::tx:
      os << "expectation::tx";
      break;
    case expectation::rx:
      os << "expectation::rx";
      break;
    case expectation::dma:
      os << "expectation::dma";
      break;
    case expectation::msix:
      os << "expectation::msix";
      break;
    case expectation::mmio:
      os << "expectation::mmio";
      break;
    default:
      os << "could not convert given 'expectation'";
      break;
  }
  return os;
}

struct Context {
  expectation expectation_;
  // NOTE: maybe include trace id, technically currently the parent span contains this information
  //       this must however be changed in case we consider distributed simulations etc.
  std::shared_ptr<event_span> parent_span_;

  inline std::shared_ptr<event_span>& get_parent() {
    return parent_span_;
  }

 private:
  Context(expectation expectation, std::shared_ptr<event_span> parent_span)
      : expectation_(expectation), parent_span_(std::move(parent_span)) {
  }

 public:
  static std::shared_ptr<Context> create(
      expectation expectation, std::shared_ptr<event_span>& parent_span) {
    throw_if_empty(parent_span, span_is_null);
    auto con = std::shared_ptr<Context>{new Context(expectation, parent_span)};
    throw_if_empty(con, context_is_null);
    return con;
  }
};

inline bool is_expectation(std::shared_ptr<Context> &con, expectation exp) {
  if (not con or con->expectation_ != exp) {
    return false;
  }
  return true;
}

// The reason why we put two lists into one queue is that always two components
// (i.e. packers that create the spans for components) interact,
// e.g. a host_packer and a nic_packer:
// 1) the host will indicate expectations by writing to queue_a and
//    the nic will see this by polling from queue_a
// 2) on the other hand will the nic indicate expectations by writing to queue_b
//    and then the host will poll from queue_b
//
// NOTE: Obviously we could give two simple list to each component, the reason
// we don't do this is that e.g. a nic has potentially two boundaries:
//   1) host simulator
//   2) network simulator / nic simulator
// When we would do it in a different way we might end up passing 4 queues to a
// nic packer, this should be prevented by this.
//
// Another reason is, that the context queue abstracts away which kind of queues
// are used internally, this might come in handy when considering distributed simulations etc.
struct ContextQueue {
 private:
  std::mutex context_queue_mutex_;

  size_t registered_spanners_ = 0;
  uint64_t spanner_a_key_ = 0;
  uint64_t spanner_b_key_ = 0;

  // spanner with key a will write to this queue
  Channel<std::shared_ptr<Context>> queue_a_;
  // spanner with key b will write to this queue
  Channel<std::shared_ptr<Context>> queue_b_;

  auto &assign_write_queue(uint64_t spanner_id) {
    std::lock_guard<std::mutex> const lock(context_queue_mutex_);

    if (spanner_id == spanner_a_key_) {
      return queue_a_;
    }

    if (spanner_id == spanner_b_key_) {
      return queue_b_;
    }

    throw_on(true, unknown_spanner_id);
    return queue_a_;
  }

  auto &assign_read_queue(uint64_t spanner_id) {
    std::lock_guard<std::mutex> const lock(context_queue_mutex_);

    if (spanner_id == spanner_a_key_) {
      return queue_b_;
    }

    if (spanner_id == spanner_b_key_) {
      return queue_a_;
    }

    throw_on(true, unknown_spanner_id);
    return queue_b_;
  }

 public:
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // TODO: remove the expectation enum, everything
  //       is expected to happen in order, hence
  //       we should be able to just pull whatever
  //       context without the expectation enum
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  void register_spanner(uint64_t spanner_id) {
    std::lock_guard<std::mutex> const lock(context_queue_mutex_);

    throw_on(registered_spanners_ == 2, already_two_spanner_registered);

    if (registered_spanners_ == 0) {
      spanner_a_key_ = spanner_id;
    } else {
      spanner_b_key_ = spanner_id;
    }
    ++registered_spanners_;
  }

  concurrencpp::result<std::shared_ptr<Context>>
  poll(std::shared_ptr<concurrencpp::executor> resume_executor, uint64_t spanner_id) {

    throw_if_empty(resume_executor, resume_executor_null);
    std::optional<std::shared_ptr<Context>> con_opt;

    // note: this function will acquire a lock
    auto &target = assign_read_queue(spanner_id);

    // the channel is safe for concurrent access itself
    con_opt = co_await target.pop(resume_executor);
    co_return con_opt.value_or(nullptr);
  }

  concurrencpp::result<std::shared_ptr<Context>>
  try_poll(std::shared_ptr<concurrencpp::executor> resume_executor, uint64_t spanner_id) {

    throw_if_empty(resume_executor, resume_executor_null);
    std::optional<std::shared_ptr<Context>> con_opt;

    // note: this function will acquire a lock
    auto &target = assign_read_queue(spanner_id);

    // the channel is safe for concurrent access itself
    con_opt = co_await target.try_pop(resume_executor);
    co_return con_opt.value_or(nullptr);
  }

  concurrencpp::result<bool>
  push(std::shared_ptr<concurrencpp::executor> resume_executor,
            uint64_t spanner_id, expectation expectation,
            std::shared_ptr<event_span> parent_span) {

    throw_if_empty(resume_executor, resume_executor_null);
    auto con = Context::create(expectation, parent_span);
    bool could_push = false;

    // note: this function will acquire a lock
    auto &target = assign_write_queue(spanner_id);

    // the channel is safe for concurrent access itself
    could_push = co_await target.push(resume_executor, con);
    co_return could_push;
  }
};

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
