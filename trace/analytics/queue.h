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

#include <iostream>
#include <list>
#include <memory>
#include <unordered_map>

#include "pack.h"

enum expectation { send, receive };

inline std::ostream &operator<<(std::ostream &os, expectation e) {
  switch (e) {
    case expectation::send:
      os << "expectation::send";
      break;
    case expectation::receive:
      os << "expectation::receive";
      break;
    default:
      os << "could not convert given 'expectation'";
      break;
  }
  return os;
}

struct context {
  expectation expectation_;
  std::shared_ptr<event_pack> parent_pack_;

  context(expectation expectation, std::shared_ptr<event_pack> parent_pack)
      : expectation_(expectation), parent_pack_(parent_pack) {
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
  std::mutex queue_mutex_;

  size_t registered_packers_ = 0;
  uint64_t packer_a_key_ = 0;
  uint64_t packer_b_key_ = 0;

  // TODO: should probably use ring buffer with condition variables!!!

  // packer with key a will write to this queue
  std::list<std::shared_ptr<context>> queue_a_;
  // packer with key b will write to this queue
  std::list<std::shared_ptr<context>> queue_b_;

  bool register_packer(uint64_t packer_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (registered_packers_ == 2) {
      return false;
    }

    if (registered_packers_ == 0) {
      packer_a_key_ = packer_id;
    } else {
      packer_b_key_ = packer_id;
    }
    ++registered_packers_;
    return true;
  }

  std::shared_ptr<context> poll(uint64_t packer_id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (packer_id == packer_a_key_ and not queue_b_.empty()) {
      auto con = queue_b_.front();
      queue_b_.pop_front();
      return con;

    } else if (packer_id == packer_b_key_ and not queue_a_.empty()) {
      auto con = queue_a_.front();
      queue_a_.pop_front();
      return con;

    } else {
      return {};
    }
  }

  bool push(uint64_t packer_id, expectation expectation,
            std::shared_ptr<event_pack> parent_pack) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    auto con = std::make_shared<context>(expectation, parent_pack);
    if (not con) {
      return false;
    }

    if (packer_id == packer_a_key_) {
      queue_a_.push_back(con);
      return true;
    } else if (packer_id == packer_b_key_) {
      queue_b_.push_back(con);
      return true;
    } else {
      return false;
    }
  }
};

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
