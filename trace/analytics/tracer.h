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

#ifndef SIMBRICKS_TRACE_TRACER_H_
#define SIMBRICKS_TRACE_TRACER_H_

#include <list>
#include <memory>
#include <unordered_map>

#include "corobelt.h"
#include "events.h"
#include "pack.h"
#include "trace.h"
#include "traceEnvironment.h"

struct tracer {
  std::mutex tracer_mutex_;

  std::unordered_map<uint64_t, std::shared_ptr<trace>> traces_;

  // add a new pack to an already existing trace
  template <class pack_type, class... Args>
  std::shared_ptr<pack_type> rergister_new_pack(uint64_t trace_id,
                                                Args&&... args) {
    // guard potential access using a lock guard
    std::lock_guard<std::mutex> lock(tracer_mutex_);

    auto it = traces_.find(trace_id);
    if (it == traces_.end()) {
      return {};
    }
    std::shared_ptr<trace> target_trace = it->second;

    std::shared_ptr<pack_type> new_pack = std::make_shared<pack_type>(args...);
    if (not new_pack) {
      return {};
    }

    // in case we did not create a new trace, we add to the current trace
    target_trace->add_pack(new_pack);
    return new_pack;
  }

  // add a pack creating a completely new trace
  template <class pack_type, class... Args>
  std::shared_ptr<pack_type> rergister_new_pack(Args&&... args) {
    // guard potential access using a lock guard
    std::lock_guard<std::mutex> lock(tracer_mutex_);

    std::shared_ptr<pack_type> new_pack = std::make_shared<pack_type>(args...);
    if (not new_pack) {
      return {};
    }

    std::shared_ptr<trace> new_trace =
        trace::create_trace(trace_environment::get_next_trace_id(), new_pack);
    if (not new_trace) {
      return {};
    }

    auto it = traces_.insert({new_trace->id_, new_trace});
    if (not it.second) {
      return {};
    }

    new_trace->add_pack(new_pack);
    return new_pack;
  }

  tracer() = default;

  ~tracer() = default;
};

#endif  // SIMBRICKS_TRACE_TRACER_H_
