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

#include "corobelt/corobelt.h"
#include "events/events.h"
#include "util/exception.h"

/* general operator to act on an event stream */
struct event_stream_actor : public cpipe<std::shared_ptr<Event>> {
  /* this method acts on an event within the stream,
   * if true is returned, the event is after acting on
   * it pased on, in case false is returned, the event
   * is filtered */
  virtual bool act_on(std::shared_ptr<Event> &event) {
    return true;
  }

  concurrencpp::result<void> process (
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan)
  override {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(tar_chan, channel_is_null);
    throw_if_empty(src_chan, channel_is_null);

    bool pass_on;
    std::shared_ptr<Event> event;
    std::optional<std::shared_ptr<Event>> msg;
    for(msg = co_await src_chan->pop(resume_executor); msg.has_value();
        msg = co_await src_chan->pop(resume_executor)) {
      event = msg.value();
      throw_if_empty(event, event_is_null);

      pass_on = act_on(event);
      if (pass_on) {
        co_await tar_chan->push(resume_executor, event);
      }
    }
    co_return;
  }

  explicit event_stream_actor() : cpipe<std::shared_ptr<Event>>() {
  }

  ~event_stream_actor() = default;
};

class GenericEventFilter : public event_stream_actor {
  std::function<bool(std::shared_ptr<Event> event)> &to_filter_;

 public:
  bool act_on(std::shared_ptr<Event> &event) override {
    return to_filter_(event);
  }

  explicit GenericEventFilter(std::function<bool(std::shared_ptr<Event> event)> &to_filter)
      : event_stream_actor(), to_filter_(to_filter) {
  }
};

class EventTypeFilter : public event_stream_actor {
  std::set<EventType> &types_to_filter_;
  bool inverted_;

 public:
  bool act_on(std::shared_ptr<Event> &event) override {
    auto search = types_to_filter_.find(event->get_type());
    const bool is_to_sink = inverted_ ? search == types_to_filter_.end()
                                      : search != types_to_filter_.end();
    return is_to_sink;
  }

  explicit EventTypeFilter(std::set<EventType> &types_to_filter, bool invert_filter)
      : event_stream_actor(),
        types_to_filter_(types_to_filter),
        inverted_(invert_filter) {
  }

  explicit EventTypeFilter(std::set<EventType> &types_to_filter)
      : event_stream_actor(),
        types_to_filter_(types_to_filter),
        inverted_(false) {
  }
};

class EventTimestampFilter : public event_stream_actor {
 public:
  struct EventTimeBoundary {
    uint64_t lower_bound_;
    uint64_t upper_bound_;

    const static uint64_t MIN_LOWER_BOUND = 0;
    const static uint64_t MAX_UPPER_BOUND = UINT64_MAX;

    explicit EventTimeBoundary(uint64_t lower_bound, uint64_t upper_bound)
        : lower_bound_(lower_bound), upper_bound_(upper_bound) {
    }
  };

 private:
  std::vector<EventTimeBoundary> &event_time_boundaries_;

 public:
  bool act_on(std::shared_ptr<Event> &event) override {
    const uint64_t ts = event->timestamp_;

    for (auto &boundary : event_time_boundaries_) {
      if (boundary.lower_bound_ <= ts && ts <= boundary.upper_bound_) {
        return true;
      }
    }
    return false;
  }

  explicit EventTimestampFilter(std::vector<EventTimeBoundary> &event_time_boundaries)
      : event_stream_actor(),
        event_time_boundaries_(event_time_boundaries) {
  }
};

#endif // SIMBRICKS_TRACE_EVENT_FILTER_H_