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

#include <condition_variable>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <unordered_map>

#include "span.h"

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

struct context {
  expectation expectation_;
  std::shared_ptr<event_span> parent_span_;

  std::shared_ptr<event_span> get_parent() {
    return parent_span_;
  }

 private:
  context(expectation expectation, std::shared_ptr<event_span> parent_span)
      : expectation_(expectation), parent_span_(parent_span) {
  }

 public:
  static std::shared_ptr<context> create(
      expectation expectation, std::shared_ptr<event_span> parent_span) {
    if (not parent_span) {
      std::cerr << "try to create context without parent span" << std::endl;
      return {};
    }

    auto con = std::shared_ptr<context>{new context(expectation, parent_span)};
    return con;
  }
};

inline bool is_expectation(std::shared_ptr<context> con, expectation exp) {
  if (not con or con->expectation_ != exp) {
    return false;
  }
  return true;
}

struct queue {
  std::mutex queue_mutex_;
  std::list<std::shared_ptr<context>> container_;
  std::condition_variable condition_v_;

  std::shared_ptr<context> poll() {
    std::unique_lock lock(queue_mutex_);
    lock.lock();

    while (container_.empty()) {
      condition_v_.wait(lock);
    }

    auto con = container_.front();
    container_.pop_front();

    lock.unlock();

    return con;
  }

  std::shared_ptr<context> try_poll() {
    std::unique_lock lock(queue_mutex_);
    lock.lock();

    std::shared_ptr<context> con;
    if (container_.empty()) {
      con = nullptr;
    } else {
      auto con = container_.front();
      container_.pop_front();
    }

    lock.unlock();

    return con;
  }

  bool push(std::shared_ptr<context> con) {
    if (not con) {
      return false;
    }

    std::unique_lock lock(queue_mutex_);
    lock.lock();

    container_.push_back(con);
    condition_v_.notify_all();

    lock.unlock();

    return true;
  }
};

// The reason why we put two lists into one queue is that always two components
// (i.e. packers that create the spans for components) interact,
// e.g. a host_packer and a nic_packer:
// 1) the host will indicate expectations by writing to queue_a and
//    the nic will see this by polling from queue_a
// 2) on the other hand will the nic indicate expectations by writing to queue_b
//    and then the host will poll from queue_b
//
// NOTE: Obviously we could give two simple list to each component, the reason
// we dont do this is that e.g. a nic has potentially two boundaries:
//   1) host simulator
//   2) network simulator / nic simulator
// When we would do it in a different way we might end up passing 4 queues to a
// nic packer, this should be prevented by this.
struct context_queue {
 private:
  std::mutex context_queue_mutex_;

  size_t registered_spanners_ = 0;
  uint64_t spanner_a_key_ = 0;
  uint64_t spanner_b_key_ = 0;

  // spanner with key a will write to this queue
  queue queue_a_;
  // spanner with key b will write to this queue
  queue queue_b_;

  queue *assign_write_queue(uint64_t spanner_id) {
    if (spanner_id == spanner_a_key_) {
      return &queue_a_;
    } else if (spanner_id == spanner_b_key_) {
      return &queue_b_;
    } else {
      return {};
    }
  }

  queue *assign_read_queue(uint64_t spanner_id) {
    if (spanner_id == spanner_a_key_) {
      return &queue_b_;

    } else if (spanner_id == spanner_b_key_) {
      return &queue_a_;

    } else {
      return {};
    }
  }

 public:
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // TODO: make smaller critical sections!!
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // TODO: remove the expectation enum, everything
  //       is expected to happen in order, hence
  //       we should be able to just pull whatever
  //       context without the expectation enum
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  bool register_spanner(uint64_t spanner_id) {
    std::lock_guard<std::mutex> lock(context_queue_mutex_);

    if (registered_spanners_ == 2) {
      return false;
    }

    if (registered_spanners_ == 0) {
      spanner_a_key_ = spanner_id;
    } else {
      spanner_b_key_ = spanner_id;
    }
    ++registered_spanners_;
    return true;
  }

  std::shared_ptr<context> poll(uint64_t spanner_id) {
    std::lock_guard<std::mutex> lock(context_queue_mutex_);

    auto target = assign_read_queue(spanner_id);
    if (not target) {
      return {};
    }

    auto con = target->poll();
    return con;
  }

  std::shared_ptr<context> try_poll(uint64_t spanner_id) {
    std::lock_guard<std::mutex> lock(context_queue_mutex_);

    auto target = assign_read_queue(spanner_id);
    if (not target) {
      return {};
    }

    auto con = target->try_poll();
    return con;
  }

  bool push(uint64_t spanner_id, expectation expectation,
            std::shared_ptr<event_span> parent_span) {
    std::lock_guard<std::mutex> lock(context_queue_mutex_);

    auto con = context::create(expectation, parent_span);
    if (not con) {
      return false;
    }

    auto target = assign_write_queue(spanner_id);
    if (not target) {
      return false;
    }

    return target->push(con);
  }
};

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
