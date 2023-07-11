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

#ifndef SIMBRICKS_TRACE_EVENT_TRACE_H_
#define SIMBRICKS_TRACE_EVENT_TRACE_H_

#include <iostream>
#include <memory>
#include <unordered_map>
#include <optional>

#include "util/exception.h"
#include "analytics/span.h"
#include "corobelt/corobelt.h"
#include "env/traceEnvironment.h"

class Trace {
  std::mutex mutex_;

  uint64_t id_;
  std::shared_ptr<EventSpan> parent_span_;
  // span_id -> span
  std::unordered_map<uint64_t, std::shared_ptr<EventSpan>> spans_;

 public:

  inline uint64_t GetId() const {
    return id_;
  }

  auto GetSpansAndRemoveSpans() {
    const std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<EventSpan>> spans;
    spans.reserve(spans_.size());
    for (const auto &iter : spans_) {
      spans.push_back(iter.second);
    }
    for (const auto &iter : spans) {
      spans_.erase(iter->GetId());
    }
    return std::move(spans);
  }

  std::shared_ptr<EventSpan> GetSpan(uint64_t span_id) {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto iter = spans_.find(span_id);
    if (iter == spans_.end()) {
      return {};
    }
    return iter->second;
  }

  bool AddSpan(std::shared_ptr<EventSpan> span) {
    throw_if_empty(span, span_is_null);

    const std::lock_guard<std::mutex> lock(mutex_);
    auto iter = spans_.insert({span->GetId(), span});
    throw_on(not iter.second, "could not insert span into spans map");
    return true;
  }

  void display(std::ostream &out) {
    const std::lock_guard<std::mutex> lock(mutex_);
    out << std::endl;
    out << "trace: id=" << id_ << std::endl;
    out << "\t parent_span:" << std::endl;
    if (parent_span_) {
      parent_span_->display(out);
    }
    for (auto &span : spans_) {
      throw_if_empty(span.second, span_is_null);
      if (span.second.get() == parent_span_.get()) {
        continue;
      }
      span.second->display(out);
    }
    out << std::endl;
  }

  Trace(uint64_t id, std::shared_ptr<EventSpan> parent_span)
      : id_(id), parent_span_(parent_span) {
    throw_if_empty(parent_span, span_is_null);
    this->AddSpan(parent_span);
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_TRACE_H_