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

#ifndef SIMBRICKS_TRACE_EVENT_FILTER_H_
#define SIMBRICKS_TRACE_EVENT_FILTER_H_

#include <memory>

#include "sync/corobelt.h"
#include "events/events.h"
#include "util/exception.h"
#include "events/eventTimeBoundary.h"

/* general operator to act on an event stream */
class EventStreamActor : public cpipe<std::shared_ptr<Event>> {

 protected:
  TraceEnvironment &trace_environment_;

 public:
  /* this method acts on an event within the stream,
   * if true is returned, the event is after acting on
   * it pased on, in case false is returned, the event
   * is filtered */
  virtual bool act_on(std::shared_ptr<Event> &event) {
    return true;
  }

  concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> src_chan,
      std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> tar_chan)
  override {
    throw_if_empty(resume_executor, TraceException::kResumeExecutorNull, source_loc::current());
    throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
    throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());

    bool pass_on;
    std::shared_ptr<Event> event;
    std::optional<std::shared_ptr<Event>> msg;
    for (msg = co_await src_chan->Pop(resume_executor); msg.has_value();
         msg = co_await src_chan->Pop(resume_executor)) {
      event = msg.value();
      throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());

      pass_on = act_on(event);
      if (pass_on) {
        co_await tar_chan->Push(resume_executor, event);
      }
    }

    co_await tar_chan->CloseChannel(resume_executor);

    co_return;
  }

  explicit EventStreamActor(TraceEnvironment &trace_environment)
      : trace_environment_(trace_environment) {
  }

  ~EventStreamActor() = default;
};

class GenericEventFilter : public EventStreamActor {
  std::function<bool(const std::shared_ptr<Event> &event)> &to_filter_;

 public:
  bool act_on(std::shared_ptr<Event> &event) override {
    return to_filter_(event);
  }

  explicit GenericEventFilter(TraceEnvironment &trace_environment,
                              std::function<bool(const std::shared_ptr<Event> &event)> &to_filter)
      : EventStreamActor(trace_environment), to_filter_(to_filter) {
  }
};

class EventTypeFilter : public EventStreamActor {
  const std::set<EventType> &types_to_filter_;
  bool inverted_;

 public:
  bool act_on(std::shared_ptr<Event> &event) override {
    const EventType type = event->GetType();
    if (inverted_) {
      return not types_to_filter_.contains(type);
    }

    return types_to_filter_.contains(type);
  }

  explicit EventTypeFilter(TraceEnvironment &trace_environment,
                           const std::set<EventType> &types_to_filter,
                           bool invert_filter = false)
      : EventStreamActor(trace_environment),
        types_to_filter_(types_to_filter),
        inverted_(invert_filter) {
  }
};

class EventTimestampFilter : public EventStreamActor {
  std::vector<EventTimeBoundary> &event_time_boundaries_;

 public:
  bool act_on(std::shared_ptr<Event> &event) override {
    const uint64_t timestamp = event->GetTs();

    for (auto &boundary : event_time_boundaries_) {
      if (boundary.lower_bound_ <= timestamp && timestamp <= boundary.upper_bound_) {
        return true;
      }
    }
    return false;
  }

  explicit EventTimestampFilter(TraceEnvironment &trace_environment,
                                std::vector<EventTimeBoundary> &event_time_boundaries)
      : EventStreamActor(trace_environment),
        event_time_boundaries_(event_time_boundaries) {
  }
};

class HostCallFuncFilter : public EventStreamActor {
  bool blacklist_;
  std::set<const std::string *> list_;

 public:

  bool act_on(std::shared_ptr<Event> &event) override {
    if (not IsType(event, EventType::kHostCallT)) {
      return true;
    }

    const std::shared_ptr<HostCall> &call = std::static_pointer_cast<HostCall>(event);
    if (blacklist_ and list_.contains(call->GetFunc())) {
      return false;
    }
    if (not blacklist_ and not list_.contains(call->GetFunc())) {
      return false;
    }

    return true;
  }

  explicit HostCallFuncFilter(TraceEnvironment &trace_environment,
                              const std::set<std::string> &list,
                              bool blacklist = true)
      : EventStreamActor(trace_environment), blacklist_(blacklist) {
    for (const std::string &str : list) {
      const std::string *sym = this->trace_environment_.InternalizeAdditional(str);
      list_.insert(sym);
    }
  }
};

#endif // SIMBRICKS_TRACE_EVENT_FILTER_H_