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

#include "corobelt/corobelt.h"
#include "events/events.h"
#include "analytics/span.h"
#include "analytics/trace.h"
#include "env/traceEnvironment.h"

struct Tracer {
  std::recursive_mutex tracer_mutex_;

  std::unordered_map<uint64_t, std::shared_ptr<Trace>> traces_;

  std::shared_ptr<Trace> get_trace(uint64_t trace_id) {
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    auto it = traces_.find(trace_id);
    if (it == traces_.end()) {
      return {};
    }
    std::shared_ptr<Trace> target_trace = it->second;
    return target_trace;
  }

  bool mark_trace_as_done(uint64_t trace_id) {
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    auto trace = get_trace(trace_id);
    throw_if_empty(trace, trace_is_null);

    trace->mark_as_done();
    //t->display(std::cout);

    return true;
  }

  bool register_span(uint64_t trace_id, std::shared_ptr<EventSpan> span_ptr) {
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    if (not span_ptr) {
      return false;
    }

    auto target_trace = get_trace(trace_id);
    throw_if_empty(target_trace, trace_is_null);
    if (target_trace->is_done()) {
      return false;
    }

    return target_trace->add_span(span_ptr);
  }

  // add a new span to an already existing trace
  template <class SpanType, class... Args>
  std::shared_ptr<SpanType> rergister_new_span(uint64_t trace_id,
                                                Args&&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    std::shared_ptr<SpanType> new_span = std::make_shared<SpanType>(args...);
    if (not new_span) {
      return {};
    }

    if (register_span(trace_id, new_span)) {
      return new_span;
    }

    return {};
  }

  // add a new span to an already existing trace and set the parent immediately
  template <class SpanType, class... Args>
  std::shared_ptr<SpanType> rergister_new_span_by_parent(
      std::shared_ptr<EventSpan> parent, Args&&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    if (not parent) {
      return {};
    }
    auto new_span =
        rergister_new_span<SpanType>(parent->get_trace_id(), args...);
    if (not new_span) {
      return {};
    }

    if (not new_span->set_parent(parent)) {
      return {};
    }
    return new_span;
  }

  // add a pack creating a completely new trace
  template <class SpanType, class... Args>
  std::shared_ptr<SpanType> rergister_new_trace(Args&&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    std::shared_ptr<SpanType> new_span = std::make_shared<SpanType>(args...);
    if (not new_span) {
      return {};
    }

    // NOTE: this will already add the pack to the trace
    std::shared_ptr<Trace> new_trace =
        Trace::create_trace(trace_environment::get_next_trace_id(), new_span);
    if (not new_trace) {
      return {};
    }

    auto it = traces_.insert({new_trace->id_, new_trace});
    if (not it.second) {
      return {};
    }
    return new_span;
  }

  Tracer() = default;

  ~Tracer() = default;
};

#endif  // SIMBRICKS_TRACE_TRACER_H_
