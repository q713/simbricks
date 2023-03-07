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

#include "trace/corobelt/coroutine.h"
#include "trace/events/events.h"

using event_t = std::shared_ptr<Event>;
using task_t = sim::coroutine::task<void>;
using chan_t = sim::coroutine::unbuffered_single_chan<event_t>;

/* general operator to act on an event stream */
struct event_stream_actor : public sim::coroutine::pipe<event_t> {
  /* this method acts on an event within the stream,
   * if true is returned, the event is after acting on
   * it pased on, in case false is returned, the event
   * is filtered */
  virtual bool act_on(event_t event) {
    return true;
  }

  task_t process(chan_t *src_chan, chan_t *tar_chan) override {
    if (not src_chan or not tar_chan) {
      co_return;
    }

    bool pass_on;
    event_t event;
    std::optional<event_t> msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      event = msg.value();
      pass_on = act_on(event);
      if (pass_on and not co_await tar_chan->write(event)) {
        break;
      }
    }

    co_return;
  }

  explicit event_stream_actor() : sim::coroutine::pipe<event_t>() {
  }

  ~event_stream_actor() = default;
};

class GenericEventFilter : public event_stream_actor {
  std::function<bool(event_t event)> &to_filter_;

 public:
  bool act_on(event_t event) override {
    return to_filter_(event);
  }

  GenericEventFilter(std::function<bool(event_t event)> &to_filter)
      : event_stream_actor(), to_filter_(to_filter) {
  }
};

class EventTypeFilter : public event_stream_actor {
  std::set<EventType> types_to_filter_;
  bool inverted_;

 public:
  bool act_on(event_t event) override {
    auto search = types_to_filter_.find(event->getType());
    bool is_to_sink = inverted_ ? search == types_to_filter_.end()
                                : search != types_to_filter_.end();
    return is_to_sink;
  }

  EventTypeFilter(std::set<EventType> types_to_filter, bool invert_filter)
      : event_stream_actor(),
        types_to_filter_(std::move(types_to_filter)),
        inverted_(invert_filter) {
  }

  EventTypeFilter(std::set<EventType> types_to_filter)
      : event_stream_actor(),
        types_to_filter_(std::move(types_to_filter)),
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
  std::vector<EventTimeBoundary> event_time_boundaries_;

 public:
  bool act_on(event_t event) override {
    uint64_t ts = event->timestamp_;
    for (auto &boundary : event_time_boundaries_) {
      if (boundary.lower_bound_ <= ts && ts <= boundary.upper_bound_) {
        return true;
      }
    }
    return false;
  }

  EventTimestampFilter(EventTimeBoundary boundary) : event_stream_actor() {
    event_time_boundaries_.push_back(std::move(boundary));
  }

  EventTimestampFilter(std::vector<EventTimeBoundary> event_time_boundaries)
      : event_stream_actor(),
        event_time_boundaries_(std::move(event_time_boundaries)) {
  }
};

#endif // SIMBRICKS_TRACE_EVENT_FILTER_H_