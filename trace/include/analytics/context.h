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
#include <mutex>

#include "util/exception.h"
#include "corobelt/corobelt.h"
#include "env/traceEnvironment.h"

enum expectation { kTx, kRx, kDma, kMsix, kMmio };

inline std::ostream &operator<<(std::ostream &out, expectation exp) {
  switch (exp) {
    case expectation::kTx:out << "expectation::tx";
      break;
    case expectation::kRx:out << "expectation::rx";
      break;
    case expectation::kDma:out << "expectation::dma";
      break;
    case expectation::kMsix:out << "expectation::msix";
      break;
    case expectation::kMmio:out << "expectation::mmio";
      break;
    default:out << "could not convert given 'expectation'";
      break;
  }
  return out;
}

class EventSpan;
std::shared_ptr<EventSpan> clone_shared(const std::shared_ptr<EventSpan> &other);

class TraceContext {
  // if parent is null it is a trace starting span
  std::shared_ptr<EventSpan> parent_ = nullptr;
  uint64_t trace_id_;
  uint64_t id_;

  std::mutex trace_context_mutex_;

 public:
  explicit TraceContext(uint64_t trace_id)
      : parent_(nullptr), trace_id_(trace_id), id_(TraceEnvironment::GetNextTraceContextId()) {
  }

  explicit TraceContext(std::shared_ptr<EventSpan> &parent, uint64_t trace_id)
      : parent_(parent), trace_id_(trace_id), id_(TraceEnvironment::GetNextTraceContextId()) {
  }

  TraceContext(const TraceContext &other) {
    trace_id_ = other.trace_id_;
    id_ = other.id_;
    parent_ = clone_shared(other.parent_);
  }

  bool HasParent() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return parent_ != nullptr;
  }

  const std::shared_ptr<EventSpan> &GetParent() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return parent_;
  }

  uint64_t GetTraceId() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return trace_id_;
  }

  uint64_t GetId() {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    return id_;
  }

  void SetTraceId(uint64_t new_id) {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    trace_id_ = new_id;
  }

  void SetParentSpan(std::shared_ptr<EventSpan> new_parent_span) {
    const std::lock_guard<std::mutex> guard(trace_context_mutex_);
    parent_ = std::move(new_parent_span);
  }

};

class Context {

  // TODO: technically the expectation is not needed, for now, it stays
  //       as it might come in handy for debugging or there like
  expectation expectation_;
  std::shared_ptr<EventSpan> parent_span_;

 public:
  Context(expectation expectation, std::shared_ptr<EventSpan> parent_span)
      : expectation_(expectation), parent_span_(parent_span) {
    throw_if_empty(parent_span, "trying to create Context, parent span is null");
  }

  inline std::shared_ptr<EventSpan> &GetParent() {
    return parent_span_;
  };

  inline std::shared_ptr<EventSpan> &GetNonEmptyParent() {
    throw_if_empty(parent_span_, "GetNonEmptyParent parent is null");
    return parent_span_;
  };

  inline expectation GetExpectation() const {
    return expectation_;
  }

};

inline std::shared_ptr<TraceContext> clone_shared(const std::shared_ptr<TraceContext> &other) {
  throw_if_empty(other, context_is_null);
  auto new_con = create_shared<TraceContext>(context_is_null, *other);
  return new_con;
}

inline bool is_expectation(std::shared_ptr<Context> &con, expectation exp) {
  if (not con or con->GetExpectation() != exp) {
    return false;
  }
  return true;
}

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
