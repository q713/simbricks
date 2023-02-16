/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software withos restriction, including
 * withos limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_

#include "trace/events/events.h"
#include "trace/corobelt/coroutine.h"

using event_t = std::shared_ptr<Event>;
using task_t = sim::coroutine::task<void>;
using chan_t = sim::coroutine::unbuffered_single_chan<event_t>;

class EventPrinter : public sim::coroutine::consumer<event_t> {
 public:
  task_t consume(chan_t *src_chan) override {
    if (!src_chan) {
      co_return;
    }
    std::optional<event_t> msg;
    for (msg = co_await src_chan->read(); msg; msg = co_await src_chan->read()) {
      std::cout << *(msg.value()) << std::endl;
    }
    co_return;
  }
};

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
    for (msg = co_await src_chan->read(); msg; msg = co_await src_chan->read()) {
      event = msg.value();
      pass_on = act_on(event);
      if (pass_on and not co_await tar_chan->write(event)) {
        break;
      }
    }

    co_return;
  }
  
  explicit event_stream_actor() : sim::coroutine::pipe<event_t>() {}

  ~event_stream_actor() = default;

};

class GenericEventFilter : public event_stream_actor {
  std::function<bool(event_t event)> &to_filter_;

 public:
  bool act_on(event_t event) override {
    return to_filter_(event);
  }

  GenericEventFilter(
      std::function<bool(event_t event)> &to_filter)
      : event_stream_actor(), to_filter_(to_filter) {
  }
};

class EventTypeFilter : public event_stream_actor {
  std::set<EventType> types_to_filter_;
  bool inverted_;

 public:
  bool act_on(event_t event) override  {
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
      : event_stream_actor(), types_to_filter_(std::move(types_to_filter)), inverted_(false) {
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

class EventTypeStatistics : public event_stream_actor {
 public:
  struct EventStat {
    uint64_t last_ts_;
    uint64_t first_ts_;
    uint64_t event_count_;
    const std::string name_;

    EventStat(const std::string name)
        : last_ts_(0), first_ts_(0), event_count_(0), name_(std::move(name)) {
    }

    friend std::ostream &operator<<(std::ostream &out, EventStat &statistic) {
      out << "typeinfo name:" << statistic.name_ << std::endl;
      out << "last_ts: " << std::to_string(statistic.last_ts_) << std::endl;
      out << "first_ts: " << std::to_string(statistic.first_ts_) << std::endl;
      out << "event_count: " << std::to_string(statistic.event_count_)
          << std::endl;
      return out;
    }
  };

 private:
  std::set<EventType> types_to_gather_statistic_;

  // TODO: map from event_type,simulator -> statistics
  // NOTE that this is also possible with the pipeline features by gathering
  // statistics before merging event streams of different simulators
  uint64_t total_event_count_;
  std::map<EventType, std::shared_ptr<EventStat>> statistics_by_type_;

  bool update_statistics(std::shared_ptr<Event> event_ptr) {
    EventType key = event_ptr->getType();
    std::shared_ptr<EventStat> statistic;
    const auto &statistics_search = statistics_by_type_.find(key);
    if (statistics_search == statistics_by_type_.end()) {
      statistic = std::make_shared<EventStat>(event_ptr->getName());
      if (statistic) {
        statistic->first_ts_ = event_ptr->timestamp_;
        auto success = statistics_by_type_.insert(
            std::make_pair(key, std::move(statistic)));
        if (!success.second) {
          return false;
        }
        statistic = success.first->second;
      } else {
        return false;
      }
    } else {
      statistic = statistics_search->second;
    }

    if (statistic == nullptr) {
      return false;
    }

    statistic->event_count_ = statistic->event_count_ + 1;
    statistic->last_ts_ = event_ptr->timestamp_;

    return true;
  }

 public:
  explicit EventTypeStatistics()
      : event_stream_actor(), total_event_count_(0) {
  }

  explicit EventTypeStatistics(std::set<EventType> types_to_gather_statistic)
      : event_stream_actor(),
        types_to_gather_statistic_(std::move(types_to_gather_statistic)),
        total_event_count_(0) {
  }

  ~EventTypeStatistics() = default;

   bool act_on(event_t event) override {
    auto search = types_to_gather_statistic_.find(event->getType());
    if (types_to_gather_statistic_.empty() or
      search != types_to_gather_statistic_.end()) {
      if (not update_statistics(event)) {
        #ifdef DEBUG_EVENT_
        DFLOGWARN("statistics for event with name %s could not be updated\n",
                event->getName().c_str());
        #endif
      }
    }
    total_event_count_ = total_event_count_ + 1;
    return true;
   }

  friend std::ostream &operator<<(std::ostream &out,
                                  EventTypeStatistics &eventTypeStatistics) {
    out << "EventTypeStatistics:" << std::endl;
    out << "a total of "
        << std::to_string(eventTypeStatistics.total_event_count_)
        << " events were counted" << std::endl;
    std::shared_ptr<EventStat> statistic_ptr;
    const std::map<EventType, std::shared_ptr<EventStat>> &statistics =
        eventTypeStatistics.getStatistics();
    if (statistics.empty()) {
      out << "no detailed statistics were gathered" << std::endl;
      return out;
    }

    for (auto it = statistics.begin(); it != statistics.end(); it++) {
      statistic_ptr = it->second;
      EventStat statistic(*statistic_ptr);
      out << std::endl;
      out << statistic;
      out << std::endl;
    }
    return out;
  }

  const std::map<EventType, std::shared_ptr<EventStat>> &getStatistics() {
    return statistics_by_type_;
  }

  std::optional<std::shared_ptr<EventStat>> getStatistic(EventType type) {
    auto statistic_search = statistics_by_type_.find(type);
    if (statistic_search != statistics_by_type_.end()) {
      return std::make_optional(statistic_search->second);
    }
    return std::nullopt;
  }
};

/* Indicator where in the events/calls are happening */
enum stack_stat_comp {
  KERNEL_NET_STACK,
  NIC_DRIVER,
  NIC_DEVICE
};

struct stack_statistics : public event_stream_actor {

  struct stack_task { // TODO: change naming
    std::vector<event_t> contributors_;

    friend std::ostream &operator<<(std::ostream &out, stack_task &task) {
      return out;
    }
  };

  // the statistics accumulated for a stack e.g. kernel network stack
  struct stack_stat { 
    std::vector<stack_task> contributing_tasks_;
  };

  bool act_on(event_t event) override {
    // TODO: implement
    return true;
  }

  explicit stack_statistics() : event_stream_actor() {} 

  ~stack_statistics() = default;

  friend std::ostream &operator<<(std::ostream &out, stack_statistics &stats) {
    // TODO: implement me
    return out;
  }

};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
