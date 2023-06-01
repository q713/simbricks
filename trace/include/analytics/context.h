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

#include "analytics/span.h"
#include "util/exception.h"
#include "corobelt/corobelt.h"

enum expectation { tx, rx, dma, msix, mmio };

inline std::ostream &operator<<(std::ostream &os, expectation e) {
  switch (e) {
    case expectation::tx:os << "expectation::tx";
      break;
    case expectation::rx:os << "expectation::rx";
      break;
    case expectation::dma:os << "expectation::dma";
      break;
    case expectation::msix:os << "expectation::msix";
      break;
    case expectation::mmio:os << "expectation::mmio";
      break;
    default:os << "could not convert given 'expectation'";
      break;
  }
  return os;
}

struct Context {
  expectation expectation_;
  // NOTE: maybe include trace id, technically currently the parent span contains this information
  //       this must however be changed in case we consider distributed simulations etc.
  std::shared_ptr<EventSpan> parent_span_;

  inline std::shared_ptr<EventSpan> &get_parent() {
    return parent_span_;
  }

 private:
  Context(expectation expectation, std::shared_ptr<EventSpan> parent_span)
      : expectation_(expectation), parent_span_(std::move(parent_span)) {
  }

 public:
  static std::shared_ptr<Context> create(
      expectation expectation, std::shared_ptr<EventSpan> parent_span) {
    throw_if_empty(parent_span, span_is_null);
    auto con = std::shared_ptr<Context>{new Context(expectation, parent_span)};
    throw_if_empty(con, context_is_null);
    return con;
  }

  void display(std::ostream &out) {
    out << "Context: " << std::endl;
    out << "expectation=" << expectation_ << std::endl;
    out << "parent span=" << *parent_span_ << std::endl;
  }
};

inline std::ostream &operator<<(std::ostream &out, Context &con) {
  con.display(out);
  return out;
}

inline bool is_expectation(std::shared_ptr<Context> &con, expectation exp) {
  if (not con or con->expectation_ != exp) {
    return false;
  }
  return true;
}

#endif  // SIMBRICKS_TRACE_CONTEXT_QUEUE_H_
