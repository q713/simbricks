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
  // std::unordered_map<uint64_t, std::shared_ptr<TraceContext>> contexts_;

  // span ids that were already exported
  //std::set<uint64_t> already_exported_spans_;
  std::set<uint64_t> exported_spans_;
  // parent_span_id -> list/vector of spans that wait for the parent to be exported
  std::unordered_map<uint64_t, std::vector<std::shared_ptr<EventSpan>>> waiting_list_;

  simbricks::trace::SpanExporter &exporter_;
  using ExportTastT = const std::function<concurrencpp::null_result()>;

  void InsertExportedSpan(uint64_t span_id) {
    throw_on_false(TraceEnvironment::IsValidId(span_id),
                   TraceException::kInvalidId, source_loc::current());
    auto iter = exported_spans_.insert(span_id);
    throw_on_false(iter.second, "could not insert non exported span into set",
                   source_loc::current());
  }

  void InsertTrace(std::shared_ptr<Trace> &new_trace) {
    // NOTE: the lock must be held when calling this method
    assert(new_trace);
    auto iter = traces_.insert({new_trace->GetId(), new_trace});
    throw_on(not iter.second, "could not insert trace into traces map",
             source_loc::current());
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

  void RemoveTrace(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    const size_t amount_removed = traces_.erase(trace_id);
    throw_on(amount_removed == 0, "RemoveTrace: nothing was removed",
             source_loc::current());
  }

  void AddSpanToTraceIfTraceExists(uint64_t trace_id, const std::shared_ptr<EventSpan> &span_ptr) {
    // NOTE: lock must be held when calling this method
    assert(span_ptr);
    auto target_trace = GetTrace(trace_id);
    if (nullptr == target_trace) {
      return;
    }
    assert(target_trace);
    target_trace->AddSpan(span_ptr);
  }

  bool SafeToDeleteTrace(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    auto trace = GetTrace(trace_id);
    if (nullptr == trace) {
      return false;
    }
    auto span_ids = trace->GetSpanIds();
    //const std::function<bool(uint64_t)> is_non_exported = [this](uint64_t span_id) {
    //  return non_exported_spans_.contains(span_id);
    //};
    //return has_non_exported;
    //const bool has_non_exported = std::ranges::none_of(span_ids, is_non_exported);
    for (auto span_id : span_ids) {
      if (not exported_spans_.contains(span_id)) {
        return false;
      }
    }
    return true;
  }

  // send
// -> bla bla bla
// --> mmiow
// ---> bla bla bla
// ---> nic tx
// ----> nic rx
// -----> bla bla bla
// -----> msix
// ------> syscall rx <-------------------------------------------
//          |                                                    |
//          -----> in actual event log we have a case that:      |
//                    |                                          |
//                    ---> return to user space                  |
//                    ---> mmio operation --> who is the parent? |--> need to set the correct TRACE id / context ---> don't know when these events will occur
//                    ---> entry new syscall

  // void InsertContext(std::shared_ptr<TraceContext> &trace_context) {
  //   // NOTE: the lock must be held when calling this method
  //   auto iter = contexts_.insert({trace_context->GetId(), trace_context});
  //   throw_on(not iter.second, "could not insert context into contexts map",
  //            source_loc::current());
  // }

  // void RemoveContext(uint64_t context_id) {
  //   // NOTE: lock must be held when calling this method
  //   const size_t amount_removed = contexts_.erase(context_id);
  //   throw_on(amount_removed == 0, "RemoveContext: nothing was removed",
  //            source_loc::current());
  // }

  // std::shared_ptr<TraceContext> GetContext(uint64_t trace_context_id) {
  //   // NOTE: lock must be held when calling this method
  //   auto iter = contexts_.find(trace_context_id);
  //   if (iter == contexts_.end()) {
  //     return {};
  //   }
  //
  //   return iter->second;
  // }

  std::shared_ptr<TraceContext>
  RegisterCreateContextParent(uint64_t trace_id, uint64_t parent_id, uint64_t parent_starting_ts) {
    // NOTE: lock must be held when calling this method
    throw_on_false(TraceEnvironment::IsValidId(trace_id),
                   TraceException::kInvalidId, source_loc::current());
    throw_on_false(TraceEnvironment::IsValidId(parent_id),
                   TraceException::kInvalidId, source_loc::current());
    auto trace_context = create_shared<TraceContext>(
        "RegisterCreateContext couldnt create context",
        trace_id, trace_environment_.GetNextTraceContextId(), parent_id, parent_starting_ts);
    // InsertContext(trace_context);
    return trace_context;
  }

  std::shared_ptr<TraceContext>
  RegisterCreateContext(uint64_t trace_id) {
    // NOTE: lock must be held when calling this method
    auto trace_context = create_shared<TraceContext>(
        "RegisterCreateContext couldnt create context",
        trace_id, trace_environment_.GetNextTraceContextId());
    // InsertContext(trace_context);
    return trace_context;
  }

  bool WasParentExported(const std::shared_ptr<EventSpan> &child) {
    // NOTE: lock must be held when calling this method
    assert(child and "span is null");
    if (not child->HasParent()) {
      // no parent means trace starting span
      return true;
    }
    const uint64_t parent_id = child->GetValidParentId();
    return exported_spans_.contains(parent_id);
  }

  void MarkSpanAsExported(std::shared_ptr<EventSpan> &span) {
    // NOTE: lock must be held when calling this method
    assert(span and "span is null");
    const uint64_t ident = span->GetId();
    InsertExportedSpan(ident);
  }

  void MarkSpanAsWaitingForParent(std::shared_ptr<EventSpan> &span) {
    // NOTE: lock must be held when calling this method
    assert(span);
    if (not span->HasParent()) {
      // we cannot wait for a non-existing parent...
      return;
    }
    //auto parent = span->GetParent();
    //assert(parent);
    const uint64_t parent_id = span->GetValidParentId();
    auto iter = waiting_list_.find(parent_id);
    if (iter != waiting_list_.end()) {
      auto &wait_vec = iter->second;
      wait_vec.push_back(span);
    } else {
      auto res_p = waiting_list_.insert({parent_id, {span}});
      throw_on(not res_p.second, "MarkSpanAsWaitingForParent: could not insert value",
               source_loc::current());
    }
  }

  // NOTE: shall only be used via 'RunExportJobInBackground'
  // NOTE: This is an eager fire and forget task, as such,
  //       the calling thread should and must not wait for
  //       consuming any result;
  //concurrencpp::null_result ExportTask(std::shared_ptr<EventSpan> span_to_export) {
  //  try {
  //    exporter_.ExportSpan(std::move(span_to_export));
  //  } catch (std::runtime_error &err) {
  //    std::cerr << "error while trying to export a trace: " << err.what() << '\n';
  //  }
  //  co_return;
  //}

  void RunExportJobInBackground(const std::shared_ptr<EventSpan> &span_to_export) {
    // NOTE: lock must be held when calling this method
    assert(span_to_export);

    ExportTastT export_task = [this, must_go = span_to_export]() -> concurrencpp::null_result {
      try {
        exporter_.ExportSpan(must_go);
      } catch (std::runtime_error &err) {
        std::cerr << "error while trying to export a trace: " << err.what() << '\n';
      }
      co_return;
    };

    auto executor = trace_environment_.GetBackgroundPoolExecutor();
    executor->submit(export_task);
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
      throw_on_false(exported_spans_.contains(waiter->GetValidParentId()),
                     "try to export span whos parent was not exported yet",
                     source_loc::current());
      MarkSpanAsExported(waiter);
      exporter_.ExportSpan(waiter);
      //RunExportJobInBackground(waiter);
      ExportWaitingForParentVec(waiter);
    }
    waiting_list_.erase(parent_id);
  }

  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanByParentInternal(uint64_t trace_id,
                                                      uint64_t parent_id,
                                                      uint64_t parent_starting_ts,
      //const std::shared_ptr<ContextType> &parent_context,
      //const std::shared_ptr<EventSpan> &parent_span,
                                                      std::shared_ptr<Event> starting_event,
                                                      Args &&... args) {
    // NOTE: lock must be held when calling this method
    assert(starting_event);
    throw_on_false(TraceEnvironment::IsValidId(trace_id), "invalid id",
                   source_loc::current());
    throw_on_false(TraceEnvironment::IsValidId(parent_id), "invalid id",
                   source_loc::current());

    //auto parent_context = parent_span->GetContext();
    auto trace_context = RegisterCreateContextParent(trace_id, parent_id, parent_starting_ts);

    auto new_span = create_shared<SpanType>(
        "StartSpanByParentInternal(...) could not create a new span",
        trace_environment_, trace_context, args...);
    const bool was_added = new_span->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanByParentInternal(...) could not add first event",
             source_loc::current());

    // must add span to trace manually
    AddSpanToTraceIfTraceExists(trace_id, new_span);

    return new_span;
  }

  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanInternal(std::shared_ptr<Event> starting_event,
                                              Args &&... args) {
    // NOTE: lock must be held when calling this method
    assert(starting_event);

    uint64_t trace_id = trace_environment_.GetNextTraceId();
    auto trace_context = RegisterCreateContext(trace_id);

    auto new_span = create_shared<SpanType>(
        "StartSpanInternal(...) could not create a new span",
        trace_environment_, trace_context, args...);
    const bool was_added = new_span->AddToSpan(starting_event);
    throw_on(not was_added, "StartSpanInternal(...) could not add first event",
             source_loc::current());

    auto new_trace = create_shared<Trace>(
        "StartSpanInternal(...) could not create a new trace", trace_id, new_span);
    InsertTrace(new_trace);

    return new_span;
  }

 public:
  void MarkSpanAsDone(std::shared_ptr<EventSpan> span) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(span, "MarkSpanAsDone, span is null", source_loc::current());
    auto context = span->GetContext();
    throw_if_empty(context, "MarkSpanAsDone context is null", source_loc::current());
    const uint64_t trace_id = context->GetTraceId();

    if (WasParentExported(span)) {
      MarkSpanAsExported(span);
      exporter_.ExportSpan(span);
      //RunExportJobInBackground(span);
      ExportWaitingForParentVec(span);

      auto trace = GetTrace(trace_id);
      if (nullptr != trace and SafeToDeleteTrace(trace_id)) {
        RemoveTrace(trace_id);
      }

      return;
    }

    assert(span->HasParent());
    MarkSpanAsWaitingForParent(span);
  }

  void AddParentLazily(const std::shared_ptr<EventSpan> &span,
                       const std::shared_ptr<Context> &parent_context) {// std::shared_ptr<EventSpan> &parent_span) {
    throw_if_empty(span, TraceException::kSpanIsNull, source_loc::current());
    //throw_if_empty(parent_span, TraceException::kSpanIsNull, source_loc::current());
    throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());

    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    //auto parent_context = parent_span->GetContext();
    //throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());
    //auto parent_trace = GetTrace(parent_context->GetTraceId());
    auto new_trace_id = parent_context->GetTraceId();
    auto old_context = span->GetContext();
    throw_if_empty(old_context, TraceException::kContextIsNull, source_loc::current());

    // NOTE: as this case shall only happen when this span is the trace root, we expect the old trace
    //       definitely to exist
    auto old_trace = GetTrace(old_context->GetTraceId());
    throw_if_empty(old_trace, TraceException::kTraceIsNull, source_loc::current());

    old_context->SetTraceId(new_trace_id);
    // old_context->SetParentSpan(parent_span);
    const uint64_t parent_id = parent_context->GetParentId();
    const uint64_t parent_start_ts = parent_context->GetParentStartingTs();
    old_context->SetParentIdAndTs(parent_id, parent_start_ts);
    AddSpanToTraceIfTraceExists(new_trace_id, span);

    auto children = old_trace->GetSpansAndRemoveSpans();
    for (const auto &iter : children) {
      if (iter->GetId() == span->GetId()) {
        continue;
      }
      iter->GetContext()->SetTraceId(new_trace_id);
      AddSpanToTraceIfTraceExists(new_trace_id, iter);
    }

    RemoveTrace(old_trace->GetId());
  }

  // will create and add a new span to a trace using the context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanByParent(//uint64_t trace_id,
      //uint64_t parent_id,
      //uint64_t parent_starting_ts,
      std::shared_ptr<EventSpan> parent_span,
      std::shared_ptr<Event> starting_event,
      Args &&... args) {
    throw_if_empty(starting_event, TraceException::kEventIsNull, source_loc::current());
    throw_if_empty(parent_span, TraceException::kSpanIsNull, source_loc::current());

    const uint64_t trace_id = parent_span->GetValidTraceId();
    const uint64_t parent_id = parent_span->GetValidId();
    const uint64_t parent_starting_ts = parent_span->GetStartingTs();

    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    std::shared_ptr<SpanType> new_span;
    new_span = StartSpanByParentInternal<SpanType, Args...>(
        trace_id, parent_id, parent_starting_ts, starting_event, std::forward<Args>(args)...);

    assert(new_span);
    return new_span;
  }

  // will create and add a new span to a trace using the context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpanByParentPassOnContext(const std::shared_ptr<Context> &parent_context,
      //std::shared_ptr<EventSpan> parent_span,
                                                           std::shared_ptr<Event> starting_event,
                                                           Args &&... args) {
    throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());
    throw_if_empty(starting_event, TraceException::kEventIsNull, source_loc::current());
    throw_on_false(parent_context->HasParent(), "Context has no parent",
                   source_loc::current());

    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    const uint64_t trace_id = parent_context->GetTraceId();
    const uint64_t parent_id = parent_context->GetParentId();
    const uint64_t parent_starting_ts = parent_context->GetParentStartingTs();
    std::shared_ptr<SpanType> new_span;
    new_span = StartSpanByParentInternal<SpanType, Args...>(
        trace_id, parent_id, parent_starting_ts, starting_event, std::forward<Args>(args)...);

    assert(new_span);
    return new_span;
  }

  // will start and create a new trace creating a new context
  template<class SpanType, class... Args>
  std::shared_ptr<SpanType> StartSpan(std::shared_ptr<Event> starting_event,
                                      Args &&... args) {
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    throw_if_empty(starting_event, "StartSpan(...) starting_event is null",
                   source_loc::current());
    std::shared_ptr<SpanType> new_span = nullptr;
    new_span = StartSpanInternal<SpanType, Args...>(starting_event, std::forward<Args>(args)...);
    assert(new_span);
    return new_span;
  }

  template<class ContextType>
  requires ContextInterface<ContextType>
  void StartSpanSetParentContext(std::shared_ptr<EventSpan> &span_to_register,
                                 const std::shared_ptr<ContextType> &parent_context) {
    //std::shared_ptr<EventSpan> &parent_span) {
    throw_if_empty(parent_context, TraceException::kContextIsNull, source_loc::current());
    // guard potential access using a lock guard
    const std::lock_guard<std::recursive_mutex> lock(tracer_mutex_);

    //throw_if_empty(parent_span, "StartSpan(...) parent span is null", source_loc::current());
    //auto parent_context = parent_span->GetContext();
    //throw_if_empty(parent_context, "StartSpan(...) parent context is null",
    //               source_loc::current());
    const uint64_t trace_id = parent_context->GetTraceId();
    const uint64_t parent_id = parent_context->GetParentId();
    const uint64_t parent_starting_ts = parent_context->GetParentStartingTs();
    auto trace_context = RegisterCreateContextParent(trace_id, parent_id, parent_starting_ts);
    span_to_register->SetContext(trace_context, true);

    // must add span to trace manually
    AddSpanToTraceIfTraceExists(trace_id, span_to_register);
  }

  explicit Tracer(TraceEnvironment &trace_environment,
                  simbricks::trace::SpanExporter &exporter)
      : trace_environment_(trace_environment), exporter_(exporter) {};

  ~Tracer() = default;
};

#endif  // SIMBRICKS_TRACE_TRACER_H_
