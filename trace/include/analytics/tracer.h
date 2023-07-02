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
#include "exporter/exporter.h"
#include "util/factory.h"
#include "analytics/context.h"

class Tracer {
  std::recursive_mutex tracer_mutex_;

  // trace_id -> trace
  std::unordered_map<uint64_t, std::shared_ptr<Trace>> traces_;
  // context_id -> context
  std::unordered_map<uint64_t, std::shared_ptr<TraceContext>> contexts_;

  simbricks::trace::SpanExporter &exporter_;

  void InsertTrace(std::shared_ptr<Trace> &new_trace) {
    // NOTE: the lock must be held when calling this method
    auto iter = traces_.insert({new_trace->GetId(), new_trace});
    throw_on(not iter.second, "could not insert trace into traces map");
  }

  std::shared_ptr<Trace> GetTrace(uint64_t trace_id) {
    // NOTE: the lock must be held when calling this method
    //const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    auto it = traces_.find(trace_id);
    if (it == traces_.end()) {
      return {};
    }
    std::shared_ptr<Trace> target_trace = it->second;
    return target_trace;
  }

  void InsertContext(std::shared_ptr<TraceContext> &trace_context) {
    // NOTE: the lock must be held when calling this method
    auto iter = contexts_.insert({trace_context->GetId(), trace_context});
    throw_on(not iter.second, "could not insert context into contexts map");
  }

  std::shared_ptr<TraceContext> GetContext(uint64_t trace_context_id) {
    // NOTE: lock must be held when calling this method
    auto it = contexts_.find(trace_context_id);
    if (it == contexts_.end()) {
      return {};
    }

    return it->second;
  }

  void AddSpanToTrace(uint64_t trace_id, std::shared_ptr<EventSpan> span_ptr) {
    // NOTE: lock must be held when calling this method

    auto target_trace = GetTrace(trace_id);
    throw_if_empty(target_trace, trace_is_null);

    target_trace->AddSpan(span_ptr);
  }

  std::shared_ptr<TraceContext>
  RegisterCreateContext(uint64_t trace_id, std::shared_ptr<EventSpan> parent) {
    // NOTE: lock must be held when calling this method
    auto trace_context = create_shared<TraceContext>(
        "RegisterCreateContext couldnt create context", parent, trace_id);
    InsertContext(trace_context);
    return trace_context;
  }

 public:
  void MarkSpanAsDone(std::shared_ptr<EventSpan> span) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(span, "MarkSpanAsDone, span is null");
    auto context = span->GetContext();
    throw_if_empty(context, "MarkSpanAsDone context is null");
    const uint64_t trace_id = context->GetTraceId();

    auto trace = GetTrace(trace_id);
    throw_if_empty(trace, "MarkSpanAsDone trace is null");

    // 283866
    auto found_span = trace->GetSpan(span->GetId());
    throw_if_empty(found_span, "MarkSpanAsDone found span is null");
    found_span->MarkAsDone();

    exporter_.EndSpan(found_span);
  }

  // will create and add a new span to a trace using the context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanByParent(std::string &service_name,
                                              std::shared_ptr<EventSpan> parent_span,
                                              std::shared_ptr<Event> starting_event,
                                              Args &&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(parent_span, "StartSpan(...) parent span is null");
    auto parent_context = parent_span->GetContext();
    throw_if_empty(parent_context, "StartSpan(...) parent context is null");
    const uint64_t trace_id = parent_context->GetTraceId();

    auto trace_context = RegisterCreateContext(trace_id, parent_span);

    auto new_span = create_shared<SpanType>(
        "StartSpan(Args &&... args) could not create a new span", trace_context, args...);
    const bool was_added = std::static_pointer_cast<EventSpan>(new_span)->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanByParent(...) could not add first event");

    // must add span to trace manually
    AddSpanToTrace(trace_id, new_span);

    exporter_.StartSpan(service_name, new_span);

    return new_span;
  }

  // will start and create a new trace creating a new context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpan(std::string &service_name,
                                      std::shared_ptr<Event> starting_event,
                                      Args &&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    uint64_t trace_id = TraceEnvironment::GetNextTraceId();

    auto trace_context = RegisterCreateContext(trace_id, nullptr);

    auto new_span = create_shared<SpanType>(
        "StartSpan(...) could not create a new span", trace_context, args...);
    const bool was_added = std::static_pointer_cast<EventSpan>(new_span)->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpan(...) could not add first event");

    // span is here added to the trace
    auto new_trace = create_shared<Trace>(
        "StartSpan(...) could not create a new trace", trace_id, new_span);

    InsertTrace(new_trace);

    exporter_.StartSpan(service_name, new_span);

    return new_span;
  }

  explicit Tracer(simbricks::trace::SpanExporter &exporter) : exporter_(exporter) {};

  ~Tracer() = default;
};

#endif  // SIMBRICKS_TRACE_TRACER_H_
