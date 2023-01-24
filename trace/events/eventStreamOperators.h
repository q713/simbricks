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

#include "trace/corobelt/belt.h"
#include "trace/events/events.h"

class EventPrinter : public corobelt::Consumer<std::shared_ptr<Event>> {
 public:
  void consume(corobelt::coro_pull_t<std::shared_ptr<Event>> &source) {
    for (std::shared_ptr<Event> event : source) {
      std::cout << *event << std::endl;
    }
    return;
  }
};

class EventFilter : public corobelt::Pipe<std::shared_ptr<Event>> {
 public:
  virtual bool is_to_sink(std::shared_ptr<Event> event_ptr) {
    return true;
  }

  explicit EventFilter() : corobelt::Pipe<std::shared_ptr<Event>>() {
  }

  virtual void act_on(
      std::shared_ptr<Event> event_ptr,
      corobelt::coro_push_t<std::shared_ptr<Event>> &sink) override {
    if (is_to_sink(event_ptr)) {
      sink(event_ptr);
    }
  }
};

class GenericEventFilter : public EventFilter {
  std::function<bool(std::shared_ptr<Event> event_ptr)> &to_filter_;

 public:
  virtual bool is_to_sink(std::shared_ptr<Event> event_ptr) override {
    return to_filter_(event_ptr);
  }

  GenericEventFilter(
      std::function<bool(std::shared_ptr<Event> event_ptr)> &to_filter)
      : EventFilter(), to_filter_(to_filter) {
  }
};

class EventTypeFilter : public EventFilter {
  std::set<EventType> types_to_filter_;
  bool inverted_;

 public:
  virtual bool is_to_sink(std::shared_ptr<Event> event_ptr) override {
    auto search = types_to_filter_.find(event_ptr->getType());
    bool is_to_sink = inverted_ ? search == types_to_filter_.end()
                                : search != types_to_filter_.end();
    return is_to_sink;
  }

  EventTypeFilter(std::set<EventType> types_to_filter)
      : EventFilter(),
        types_to_filter_(std::move(types_to_filter)),
        inverted_(false) {
  }

  EventTypeFilter(std::set<EventType> types_to_filter, bool invert_filter)
      : EventFilter(),
        types_to_filter_(std::move(types_to_filter)),
        inverted_(invert_filter) {
  }
};

class EventTimestampFilter : public EventFilter {
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
  virtual bool is_to_sink(std::shared_ptr<Event> event_ptr) override {
    uint64_t ts = event_ptr->timestamp_;
    for (auto &boundary : event_time_boundaries_) {
      if (boundary.lower_bound_ <= ts && ts <= boundary.upper_bound_) {
        return true;
      }
    }
    return false;
  }

  EventTimestampFilter(EventTimeBoundary boundary) : EventFilter() {
    event_time_boundaries_.push_back(std::move(boundary));
  }

  EventTimestampFilter(std::vector<EventTimeBoundary> event_time_boundaries)
      : EventFilter(),
        event_time_boundaries_(std::move(event_time_boundaries)) {
  }
};

class HostMmioTimeStatistics : public corobelt::Pipe<std::shared_ptr<Event>> {
 public:
  explicit HostMmioTimeStatistics() : corobelt::Pipe<std::shared_ptr<Event>>() {
  }

  virtual void act_on(
      std::shared_ptr<Event> event_ptr,
      corobelt::coro_push_t<std::shared_ptr<Event>> &sink) override {
  }
};

class EventTypeStatistics : public corobelt::Pipe<std::shared_ptr<Event>> {
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
      : corobelt::Pipe<std::shared_ptr<Event>>(), total_event_count_(0) {
  }

  explicit EventTypeStatistics(std::set<EventType> types_to_gather_statistic)
      : corobelt::Pipe<std::shared_ptr<Event>>(),
        types_to_gather_statistic_(std::move(types_to_gather_statistic)),
        total_event_count_(0) {
  }

  ~EventTypeStatistics() {
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

  virtual void act_on(
      std::shared_ptr<Event> event_ptr,
      corobelt::coro_push_t<std::shared_ptr<Event>> &sink) override {
    auto search = types_to_gather_statistic_.find(event_ptr->getType());
    if (types_to_gather_statistic_.empty() ||
        search != types_to_gather_statistic_.end()) {
      if (!update_statistics(event_ptr)) {
#ifdef DEBUG_EVENT_
        DFLOGWARN("statistics for event with name %s could not be updated\n",
                  event_ptr->getName().c_str());
#endif
      }
    }
    total_event_count_ = total_event_count_ + 1;
    sink(event_ptr);
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
