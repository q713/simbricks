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
#include <set>
#include <utility>

#include "sync/corobelt.h"
#include "sync/channel.h"
#include "events/events.h"
#include "analytics/span.h"
#include "analytics/trace.h"
#include "env/traceEnvironment.h"
#include "exporter/exporter.h"
#include "util/factory.h"
#include "analytics/context.h"
#include "util/exception.h"

//template<size_t ExportBufferSize> requires SizeLagerZero<ExportBufferSize>
class Tracer {
  std::recursive_mutex tracer_mutex_;

  TraceEnvironment &trace_environment_;

  // trace_id -> trace
  std::unordered_map<uint64_t, std::shared_ptr<Trace>> traces_;
  // context_id -> context
  std::unordered_map<uint64_t, std::shared_ptr<TraceContext>> contexts_;

  // span ids that were already exported
  std::set<uint64_t> already_exported_spans_;
  // parent_span_id -> list/vector of spans that wait for the parent to be exported
  std::unordered_map<uint64_t, std::vector<std::shared_ptr<EventSpan>>> waiting_list_;

  simbricks::trace::SpanExporter &exporter_;

  void InsertTrace(std::shared_ptr<Trace> &new_trace) {
    // NOTE: the lock must be held when calling this method
    auto iter = traces_.insert({new_trace->GetId(), new_trace});
    throw_on(not iter.second, "could not insert trace into traces map");
  }

  std::shared_ptr<Trace> GetTrace(uint64_t trace_id) {
    // NOTE: the lock must be held when calling this method
    auto iter = traces_.find(trace_id);
    if (iter == traces_.end()) {
      return {};
    }
    std::shared_ptr<Trace> target_trace = iter->second;
    return target_trace;
  }

  void InsertContext(std::shared_ptr<TraceContext> &trace_context) {
    // NOTE: the lock must be held when calling this method
    auto iter = contexts_.insert({trace_context->GetId(), trace_context});
    throw_on(not iter.second, "could not insert context into contexts map");
  }

  void RemoveContext(uint64_t context_id) {
    // NOTE: lock must be held when calling this method
    const size_t amount_removed = contexts_.erase(context_id);
    throw_on(amount_removed == 0, "RemoveContext: nothing was removed");
  }

  void RemoveTrace(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    const size_t amount_removed = traces_.erase(trace_id);
    throw_on(amount_removed == 0, "RemoveTrace: nothing was removed");
  }

  std::shared_ptr<TraceContext> GetContext(uint64_t trace_context_id) {
    // NOTE: lock must be held when calling this method
    auto iter = contexts_.find(trace_context_id);
    if (iter == contexts_.end()) {
      return {};
    }

    return iter->second;
  }

  void AddSpanToTrace(uint64_t trace_id, const std::shared_ptr<EventSpan> &span_ptr) {
    // NOTE: lock must be held when calling this method

    auto target_trace = GetTrace(trace_id);
    throw_if_empty(target_trace, TraceException::kTraceIsNull);

    target_trace->AddSpan(span_ptr);
  }

  std::shared_ptr<TraceContext>
  RegisterCreateContext(uint64_t trace_id, std::shared_ptr<EventSpan> parent) {
    // NOTE: lock must be held when calling this method
    auto trace_context = create_shared<TraceContext>(
        "RegisterCreateContext couldnt create context",
        parent, trace_id, trace_environment_.GetNextTraceContextId());
    InsertContext(trace_context);
    return trace_context;
  }

  bool WasParentExported(std::shared_ptr<EventSpan> &child) {
    // NOTE: lock must be held when calling this method
    assert(child and "span is null");
    auto parent = child->GetParent();
    if (not parent) { // no parent measn trace starting span
      return true;
    }
    const uint64_t parent_id = parent->GetId();
    return already_exported_spans_.contains(parent_id);
  }

  void MarkSpanAsExported(std::shared_ptr<EventSpan> &span) {
    // NOTE: lock must be held when calling this method
    assert(span and "span is null");
    const uint64_t ident = span->GetId();
    auto res_p = already_exported_spans_.insert(ident);
    throw_on(not res_p.second, "MarkSpanAsExported: could not insert value");
  }

  void MarkSpanAsWaitingForParent(std::shared_ptr<EventSpan> &span) {
    // NOTE: lock must be held when calling this method
    assert(span and TraceException::kSpanIsNull);
    auto parent = span->GetParent();
    assert(parent and TraceException::kSpanIsNull);
    const uint64_t parent_id = parent->GetId();
    auto iter = waiting_list_.find(parent_id);
    if (iter != waiting_list_.end()) {
      auto &wait_vec = iter->second;
      wait_vec.push_back(span);
    } else {
      auto res_p = waiting_list_.insert({parent_id, {span}});
      throw_on(not res_p.second, "MarkSpanAsWaitingForParent: could not insert value");
    }
  }

  // NOTE: shall only be used via 'RunExportJobInBackground'
  // NOTE: This is an eager fire and forget task, as such,
  //       the calling thread should and must not wait for
  //       consuming any result;
  concurrencpp::null_result ExportTask(std::shared_ptr<EventSpan> span_to_export) {
    try {
      exporter_.ExportSpan(std::move(span_to_export));
    } catch (std::runtime_error &err) {
      std::cerr << "error while trying to export a trace: " << err.what() << '\n';
    }
    co_return;
  }

  void RunExportJobInBackground(std::shared_ptr<EventSpan> span_to_export) {
    // NOTE: lock must be held when calling this method
    auto executor = trace_environment_.GetBackgroundPoolExecutor();
    executor->submit(std::bind(&Tracer::ExportTask, this, std::move(span_to_export)));
  }

  void ExportWaitingForParentVec(std::shared_ptr<EventSpan> &parent) {
    // NOTE: lock must be held when calling this method
    assert(parent and "span is null");
    const uint64_t parent_id = parent->GetId();
    auto iter = waiting_list_.find(parent_id);
    if (iter == waiting_list_.end()) {
      return;
    }
    auto waiters = iter->second;
    for (auto &waiter : waiters) {
      //exporter_.ExportSpan(waiter); // TODO: use export queue vs. push job on normal thread pool / background executor
      RunExportJobInBackground(waiter);
      ExportWaitingForParentVec(waiter);
      MarkSpanAsExported(waiter);
    }
    waiting_list_.erase(parent_id);
  }

  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanByParentInternal(const std::shared_ptr<EventSpan> &parent_span,
                                                      std::shared_ptr<Event> starting_event,
                                                      Args &&... args) {
    // NOTE: lock must be held when calling this method
    assert(parent_span);
    assert(starting_event);

    auto parent_context = parent_span->GetContext();
    const uint64_t trace_id = parent_context->GetTraceId();
    auto trace_context = RegisterCreateContext(trace_id, parent_span);

    auto new_span = create_shared<SpanType>(
        "StartSpanByParentInternal(...) could not create a new span",
        trace_environment_, trace_context, args...);
    const bool was_added = std::static_pointer_cast<EventSpan>(new_span)->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanByParentInternal(...) could not add first event");

    // must add span to trace manually
    AddSpanToTrace(trace_id, new_span);

    return new_span;
  }

  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanInternal(std::shared_ptr<Event> starting_event,
                                              Args &&... args) {
    // NOTE: lock must be held when calling this method
    assert(starting_event);

    uint64_t trace_id = trace_environment_.GetNextTraceId();
    auto trace_context = RegisterCreateContext(trace_id, nullptr);

    auto new_span = create_shared<SpanType>(
        "StartSpanInternal(...) could not create a new span",
        trace_environment_, trace_context, args...);
    const bool was_added = std::static_pointer_cast<EventSpan>(new_span)->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanInternal(...) could not add first event");

    // TODO: not needed
    auto new_trace = create_shared<Trace>(
        "StartSpanInternal(...) could not create a new trace", trace_id, new_span);
    InsertTrace(new_trace);

    return new_span;
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

    if (WasParentExported(span)) {
      // TODO: make this run concurrently, either within exporter or within tracer
      //exporter_.ExportSpan(span); // TODO: use export queue vs. push job on normal thread pool / background executor
      RunExportJobInBackground(span);
      ExportWaitingForParentVec(span);
      MarkSpanAsExported(span);
      return;
    }

    MarkSpanAsWaitingForParent(span);
  }

  void AddParentLazily(const std::shared_ptr<EventSpan> &span, std::shared_ptr<EventSpan> &parent_span) {
    throw_if_empty(span, TraceException::kSpanIsNull);
    throw_if_empty(parent_span, TraceException::kSpanIsNull);

    auto parent_context = parent_span->GetContext();
    throw_if_empty(parent_context, TraceException::kContextIsNull);
    auto parent_trace = GetTrace(parent_context->GetTraceId());
    auto new_trace_id = parent_context->GetTraceId();

    auto old_context = span->GetContext();
    throw_if_empty(old_context, TraceException::kContextIsNull);
    auto old_trace = GetTrace(old_context->GetTraceId());
    throw_if_empty(old_trace, TraceException::kTraceIsNull);

    old_context->SetTraceId(new_trace_id);
    old_context->SetParentSpan(parent_span);
    AddSpanToTrace(new_trace_id, span);

    auto childs = old_trace->GetSpansAndRemoveSpans();
    for (const auto &iter : childs) {
      if (iter->GetId() == span->GetId()) {
        continue;
      }
      iter->GetContext()->SetTraceId(new_trace_id);
      AddSpanToTrace(new_trace_id, iter);
    }

    RemoveTrace(old_trace->GetId());
  }

  // will create and add a new span to a trace using the context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanByParent(std::shared_ptr<EventSpan> parent_span,
                                              std::shared_ptr<Event> starting_event,
                                              Args &&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(starting_event, "StartSpanByParent(...) starting_event is null");
    std::shared_ptr<SpanType> new_span;

    if (parent_span) {
      throw_if_empty(parent_span->GetContext(),
                     "StartSpanByParent(...) parent context is null");
      new_span = StartSpanByParentInternal<SpanType, Args...>(
          parent_span, starting_event, std::forward<Args>(args)...);
    } else {
      //std::cout << "StartSpanByParent: fallback to trace starting"
      //             " span as no parent was given" << std::endl;

      new_span = StartSpanInternal<SpanType, Args...>(starting_event,
                                                      std::forward<Args>(args)...);
    }

    assert(new_span);
    return new_span;
  }

  // will start and create a new trace creating a new context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpan(std::shared_ptr<Event> starting_event,
                                      Args &&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(starting_event, "StartSpan(...) starting_event is null");
    std::shared_ptr<SpanType> new_span = nullptr;
    new_span = StartSpanInternal<SpanType, Args...>(starting_event, std::forward<Args>(args)...);
    assert(new_span);
    return new_span;
  }

  void StartSpanSetParentContext(std::shared_ptr<EventSpan> &span_to_register,
                                 std::shared_ptr<EventSpan> &parent_span) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(parent_span, "StartSpan(...) parent span is null");
    auto parent_context = parent_span->GetContext();
    throw_if_empty(parent_context, "StartSpan(...) parent context is null");
    const uint64_t trace_id = parent_context->GetTraceId();

    auto trace_context = RegisterCreateContext(trace_id, parent_span);
    span_to_register->SetContext(trace_context, true);

    // must add span to trace manually
    AddSpanToTrace(trace_id, span_to_register);
  }

  explicit Tracer(TraceEnvironment &trace_environment,
                  simbricks::trace::SpanExporter &exporter)
      : trace_environment_(trace_environment), exporter_(exporter) {};

  ~Tracer() = default;
};

#endif  // SIMBRICKS_TRACE_TRACER_H_
