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
#include <vector>
#include <optional>

#include "util/exception.h"
#include "analytics/span.h"
#include "corobelt/corobelt.h"
#include "env/traceEnvironment.h"

struct trace {
  std::mutex mutex_;

  uint64_t id_;
  std::shared_ptr<event_span> parent_span_;

  // TODO: maybe store spans by source id...
  std::vector<std::shared_ptr<event_span>> spans_;

  bool is_done_ = false;

  inline bool is_done() const {
    return is_done_;
  }

  inline void mark_as_done() {
    is_done_ = true;
  }

  bool add_span(std::shared_ptr<event_span> span) {
    throw_if_empty(span, span_is_null);

    std::lock_guard<std::mutex> lock(mutex_);
    span->set_trace_id(id_);
    spans_.push_back(span);
    return true;
  }

  void display(std::ostream &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    out << std::endl;
    out << "trace: id=" << id_ << std::endl;
    for (auto span : spans_) {
      throw_if_empty(span, span_is_null);
      if (span.get() == parent_span_.get()) {
        out << "\t parent_span:" << std::endl;
        span->display(out, 1);
      }
    }
    out << std::endl;
  }

  static std::shared_ptr<trace> create_trace(
      uint64_t id, std::shared_ptr<event_span> parent_span) {
    throw_if_empty(parent_span, span_is_null);

    auto t = std::shared_ptr<trace>{new trace{id, parent_span}};
    return t;
  }

 private:
  trace(uint64_t id, std::shared_ptr<event_span> parent_span)
      : id_(id), parent_span_(parent_span) {
    this->add_span(parent_span);
  }
};


#endif  // SIMBRICKS_TRACE_EVENT_TRACE_H_