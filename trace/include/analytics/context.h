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

class TraceContext {
  // if parent is null it is a trace starting span
  std::shared_ptr<EventSpan> parent_ = nullptr;
  uint64_t trace_id_;
  uint64_t id_;

  std::mutex trace_context_mutex_;

 public:
  explicit TraceContext(uint64_t trace_id)
      : parent_(nullptr), trace_id_(trace_id), id_(trace_environment::GetNextTraceContextId()) {
  }

  explicit TraceContext(std::shared_ptr<EventSpan> &parent, uint64_t trace_id)
      : parent_(parent), trace_id_(trace_id), id_(trace_environment::GetNextTraceContextId()) {
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

};

class Context {

  expectation expectation_;
  // NOTE: maybe include trace id, technically currently the parent span contains this information
  //       this must however be changed in case we consider distributed simulations etc.
  std::shared_ptr<TraceContext> trace_context_;

 public:
  Context(expectation expectation, std::shared_ptr<TraceContext> trace_context)
      : expectation_(expectation), trace_context_(trace_context) {
    throw_if_empty(trace_context, "trying to create Context, trace context is null");
  }

  inline const std::shared_ptr<TraceContext> &GetTraceContext() const {
    return trace_context_;
  }

  inline const std::shared_ptr<TraceContext> &GetNonEmptyTraceContext() {
    throw_if_empty(trace_context_, "GetNonEmptyTraceContext trace context is null");
    return trace_context_;
  }

  inline expectation GetExpectation() const {
    return expectation_;
  }

};

inline bool is_expectation(std::shared_ptr<Context> &con, expectation exp) {
  if (not con or con->GetExpectation() != exp) {
    return false;
  }
  return true;
}

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
