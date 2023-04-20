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

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_

#include <bit>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/analytics/trace.h"
#include "trace/corobelt/corobelt.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/events.h"

using trace_t = std::shared_ptr<tcp_trace>;
using event_t = std::shared_ptr<Event>;

struct event_stream_tracer_tcp
    : public sim::corobelt::transformer<event_t, trace_t> {
  using msg_t = std::optional<event_t>;
  using src_task = sim::corobelt::yield_task<event_t>;
  using tar_task = sim::corobelt::yield_task<trace_t>;  

  sim::trace::env::trace_environment &env_;

  trace_t cur_trace_ = nullptr;

  // handling of yet unmatched events
  std::list<event_t> unmatched_events;

  bool has_unmatched() {
    return not unmatched_events.empty();
  }

  bool add_unmatched(event_t event) {
    static std::set<event_t> blacklisted_events;
    if (blacklisted_events.contains(event)) {
      std::stringstream es;
      es << *event;
      DFLOGERR("try to add unmatched yet already blacklisted event: %s",
               es.str().c_str());
      return false;
    }
    unmatched_events.push_back(event);
    return blacklisted_events.insert(event).second;
  }

  event_t get_unmatched() {
    if (unmatched_events.empty()) {
      return nullptr;
    }
    event_t event_ptr = unmatched_events.front();
    unmatched_events.pop_front();
    return event_ptr;
  }

  tar_task produce() override {
    msg_t msg;
    event_t event_ptr = nullptr;
    // hostc_t host_call = nullptr;
    bool handle_unmatched = false;

    src_task producer_task = prod_.produce();

    while (true) {
      if (cur_trace_ and not cur_trace_->is_trace_pending()) {
        DLOGIN("found one stack to finish\n");
        // finish up this stack
        handle_unmatched = true;
        co_yield cur_trace_;
        cur_trace_ = nullptr;
      }

      event_ptr = nullptr;
      if (handle_unmatched and has_unmatched()) {
        // we are in this case when we are not longer within a trace
        event_ptr = get_unmatched();
      } else {
        if (producer_task) {
          event_ptr = producer_task.get();
          handle_unmatched = false;
        } else if (has_unmatched()) {
          handle_unmatched = true;
          continue;
        } else {
          DLOGIN("no events left for processing\n");
          co_return;
        }
      }

      if (!event_ptr) {
        DLOGERR("no event left but one would be expected\n");
        if (cur_trace_) {
          co_yield cur_trace_;
        }

        co_return;
      }

      if (!cur_trace_) {
        cur_trace_ = std::make_shared<tcp_trace>(env_);
        if (!cur_trace_) {
          DLOGERR("could not allocate new trace\n");
          co_return;
        }
      }

      if (not cur_trace_->add_to_trace(event_ptr)) {
        if (not handle_unmatched) {
          add_unmatched(event_ptr);
        } else {
          DLOGWARN("found unhandled event unable to handle twice\n");
          std::cerr << *event_ptr << std::endl;
        }
      }
    }
  }

  explicit event_stream_tracer_tcp(sim::corobelt::producer<event_t> &prod,
                                   sim::trace::env::trace_environment &env)
      : sim::corobelt::transformer<event_t, trace_t>(prod), env_(env) {
  }

  ~event_stream_tracer_tcp() = default;
};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
