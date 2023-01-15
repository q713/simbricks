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

#ifndef SIMBRICKS_TRACE_EVENTS_H_
#define SIMBRICKS_TRACE_EVENTS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>

#include "lib/utils/log.h"
#include "trace/corobelt/belt.h"

#define DEBUG_EVENT_ ;

class LogParser;

enum EventType {
  Event_t,
  SimSendSync_t,
  SimProcInEvent_t,
  HostInstr_t,
  HostCall_t,
  HostMmioImRespPoW_t,
  HostIdOp_t,
  HostMmioCR_t,
  HostMmioCW_t,
  HostAddrSizeOp_t,
  HostMmioR_t,
  HostMmioW_t,
  HostDmaC_t,
  HostDmaR_t,
  HostDmaW_t,
  HostMsiX_t,
  HostConf_t,
  HostClearInt_t,
  HostPostInt_t,
  HostPciRW_t,
  NicMsix_t,
  NicDma_t,
  SetIX_t,
  NicDmaI_t,
  NicDmaEx_t,
  NicDmaEn_t,
  NicDmaCR_t,
  NicDmaCW_t,
  NicMmio_t,
  NicMmioR_t,
  NicMmioW_t,
  NicTrx_t,
  NicTx_t,
  NicRx_t
};

/* Parent class for all events of interest */
class Event {
  EventType type_;
  std::string name_;

 public:
  uint64_t timestamp_;
  LogParser *src_;

  const std::string &getName() {
    return name_;
  }

  EventType getType() {
    return type_;
  }

  virtual void display(std::ostream &os);

 protected:
  explicit Event(uint64_t ts, LogParser *src, EventType type, std::string name)
      : type_(type), name_(std::move(name)), timestamp_(ts), src_(src) {
  }
};

/* Simbricks Events */
class SimSendSync : public Event {
 public:
  explicit SimSendSync(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::SimSendSync_t, "SimSendSyncSimSendSync") {
  }

  void display(std::ostream &os) override;
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::SimProcInEvent_t, "SimProcInEvent") {
  }

  void display(std::ostream &os) override;
};

/* Host related events */

class HostInstr : public Event {
 public:
  uint64_t pc_;

  HostInstr(uint64_t ts, LogParser *src, uint64_t pc)
      : Event(ts, src, EventType::HostInstr_t, "HostInstr"), pc_(pc) {
  }

  HostInstr(uint64_t ts, LogParser *src, uint64_t pc, EventType type,
            std::string name)
      : Event(ts, src, type, name), pc_(pc) {
  }

  void display(std::ostream &os) override;
};

class HostCall : public HostInstr {
 public:
  const std::string func_;

  explicit HostCall(uint64_t ts, LogParser *src, uint64_t pc,
                    const std::string func)
      : HostInstr(ts, src, pc, EventType::HostCall_t, "HostCall"),
        func_(std::move(func)) {
  }

  void display(std::ostream &os) override;
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::HostMmioImRespPoW_t, "HostMmioImRespPoW") {
  }

  void display(std::ostream &os) override;
};

class HostIdOp : public Event {
 public:
  uint64_t id_;

  void display(std::ostream &os) override;

 protected:
  explicit HostIdOp(uint64_t ts, LogParser *src, EventType type,
                    std::string name, uint64_t id)
      : Event(ts, src, type, std::move(name)), id_(id) {
  }
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, EventType::HostMmioCR_t, "HostMmioCR", id) {
  }

  void display(std::ostream &os) override;
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, EventType::HostMmioCW_t, "HostMmioCW", id) {
  }

  void display(std::ostream &os) override;
};

class HostAddrSizeOp : public HostIdOp {
 public:
  uint64_t addr_;
  uint64_t size_;

  void display(std::ostream &os) override;

 protected:
  explicit HostAddrSizeOp(uint64_t ts, LogParser *src, EventType type,
                          std::string name, uint64_t id, uint64_t addr,
                          uint64_t size)
      : HostIdOp(ts, src, type, std::move(name), id) {
  }
};

class HostMmioR : public HostAddrSizeOp {
 public:
  explicit HostMmioR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                     uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostMmioR_t, "HostMmioR", id, addr,
                       size) {
  }

  void display(std::ostream &os) override;
};

class HostMmioW : public HostAddrSizeOp {
 public:
  explicit HostMmioW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                     uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostMmioW_t, "HostMmioW", id, addr,
                       size) {
  }

  void display(std::ostream &os) override;
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, EventType::HostDmaC_t, "HostDmaC", id) {
  }

  void display(std::ostream &os) override;
};

class HostDmaR : public HostAddrSizeOp {
 public:
  explicit HostDmaR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostDmaR_t, "HostDmaR", id, addr,
                       size) {
  }

  void display(std::ostream &os) override;
};

class HostDmaW : public HostAddrSizeOp {
 public:
  explicit HostDmaW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostDmaW_t, "HostDmaW", id, addr,
                       size) {
  }

  void display(std::ostream &os) override;
};

class HostMsiX : public Event {
 public:
  uint64_t vec_;

  explicit HostMsiX(uint64_t ts, LogParser *src, uint64_t vec)
      : Event(ts, src, EventType::HostMsiX_t, "HostMsiX"), vec_(vec) {
  }

  void display(std::ostream &os) override;
};

class HostConf : public Event {
 public:
  uint64_t dev_;
  uint64_t func_;
  uint64_t reg_;
  uint64_t bytes_;
  uint64_t data_;
  bool is_read_;

  explicit HostConf(uint64_t ts, LogParser *src, uint64_t dev, uint64_t func,
                    uint64_t reg, uint64_t bytes, uint64_t data, bool is_read)
      : Event(ts, src, EventType::HostConf_t,
              is_read ? "HostConfRead" : "HostConfWrite"),
        dev_(dev),
        func_(func),
        reg_(reg),
        bytes_(bytes),
        data_(data),
        is_read_(is_read) {
  }

  void display(std::ostream &os) override;
};

class HostClearInt : public Event {
 public:
  explicit HostClearInt(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::HostClearInt_t, "HostClearInt") {
  }

  void display(std::ostream &os) override;
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::HostPostInt_t, "HostPostInt") {
  }

  void display(std::ostream &os) override;
};

class HostPciRW : public Event {
 public:
  uint64_t offset_;
  uint64_t size_;
  bool is_read_;

  explicit HostPciRW(uint64_t ts, LogParser *src, uint64_t offset,
                     uint64_t size, bool is_read)
      : Event(ts, src, EventType::HostPciRW_t,
              is_read ? "HostPciR" : "HostPciW"),
        offset_(offset),
        size_(size),
        is_read_(is_read) {
  }

  void display(std::ostream &os) override;
};

/* NIC related events */
class NicMsix : public Event {
 public:
  uint16_t vec_;
  bool isX_;

  NicMsix(uint64_t ts, LogParser *src, uint16_t vec, bool isX)
      : Event(ts, src, EventType::NicMsix_t, isX ? "NicMsix" : "NicMsi"),
        vec_(vec),
        isX_(isX) {
  }

  void display(std::ostream &os) override;
};

class NicDma : public Event {
 public:
  uint64_t id_;
  uint64_t addr_;
  uint64_t len_;

  void display(std::ostream &os) override;

 protected:
  NicDma(uint64_t ts, LogParser *src, EventType type, std::string name,
         uint64_t id, uint64_t addr, uint64_t len)
      : Event(ts, src, type, std::move(name)), id_(id), addr_(addr), len_(len) {
  }
};

class SetIX : public Event {
 public:
  uint64_t intr_;

  SetIX(uint64_t ts, LogParser *src, uint64_t intr)
      : Event(ts, src, EventType::SetIX_t, "SetIX"), intr_(intr) {
  }

  void display(std::ostream &os) override;
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, src, EventType::NicDmaI_t, "NicDmaI", id, addr, len) {
  }

  void display(std::ostream &os) override;
};

class NicDmaEx : public NicDma {
 public:
  NicDmaEx(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaEx_t, "NicDmaEx", id, addr, len) {
  }

  void display(std::ostream &os) override;
};

class NicDmaEn : public NicDma {
 public:
  NicDmaEn(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaEn_t, "NicDmaEn", id, addr, len) {
  }

  void display(std::ostream &os) override;
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaCR_t, "NicDmaCR", id, addr, len) {
  }

  void display(std::ostream &os) override;
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaCW_t, "NicDmaCW", id, addr, len) {
  }

  void display(std::ostream &os) override;
};

class NicMmio : public Event {
 public:
  uint64_t off_;
  uint64_t len_;
  uint64_t val_;

  virtual void display(std::ostream &os) override;

 protected:
  NicMmio(uint64_t ts, LogParser *src, EventType type, std::string name,
          uint64_t off, uint64_t len, uint64_t val)
      : Event(ts, src, type, std::move(name)), off_(off), len_(len), val_(val) {
  }
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t ts, LogParser *src, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, src, EventType::NicMmioR_t, "NicMmioR", off, len, val) {
  }

  void display(std::ostream &os) override;
};

class NicMmioW : public NicMmio {
 public:
  NicMmioW(uint64_t ts, LogParser *src, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, src, EventType::NicMmioW_t, "NicMmioW", off, len, val) {
  }

  void display(std::ostream &os) override;
};

class NicTrx : public Event {
 public:
  uint16_t len_;

  void display(std::ostream &os) override;

 protected:
  NicTrx(uint64_t ts, LogParser *src, EventType type, std::string name,
         uint16_t len)
      : Event(ts, src, type, std::move(name)), len_(len) {
  }
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, LogParser *src, uint16_t len)
      : NicTrx(ts, src, EventType::NicTx_t, "NicTx", len) {
  }

  void display(std::ostream &os) override;
};

class NicRx : public NicTrx {
  uint64_t port_;

 public:
  NicRx(uint64_t ts, LogParser *src, uint64_t port, uint16_t len)
      : NicTrx(ts, src, EventType::NicRx_t, "NicRx", len), port_(port) {
  }

  void display(std::ostream &os) override;
};

inline std::ostream &operator<<(std::ostream &os, Event &e) {
  e.display(os);
  return os;
}

struct EventComperator {
  bool operator()(const std::shared_ptr<Event> &e1,
                  const std::shared_ptr<Event> &e2) const {
    return e1->timestamp_ < e2->timestamp_;
  }
};

class EventPrinter : public corobelt::Consumer<std::shared_ptr<Event>> {
 public:
  void consume(corobelt::coro_pull_t<std::shared_ptr<Event>> &source) {
    for (std::shared_ptr<Event> event : source) {
      std::cout << "EventPrinter: " << *event << std::endl;
    }
    return;
  }
};

/* Filter out all events that do not have one of the in the template specified
 * type */
class EventTypeFilter : public corobelt::Pipe<std::shared_ptr<Event>> {
  std::set<EventType> types_to_filter_;

 public:
  EventTypeFilter(std::set<EventType> types_to_filter)
      : corobelt::Pipe<std::shared_ptr<Event>>(),
        types_to_filter_(std::move(types_to_filter)) {
  }

  void process(corobelt::coro_push_t<std::shared_ptr<Event>> &sink,
               corobelt::coro_pull_t<std::shared_ptr<Event>> &source) override {
    for (std::shared_ptr<Event> event : source) {
      auto search = types_to_filter_.find(event->getType());
      if (search != types_to_filter_.end()) {
        sink(event);
      }
    }
  }
};

struct EventStat {
  uint64_t last_ts_;
  uint64_t first_ts_;
  uint64_t event_count_;
  uint64_t min_time_;
  uint64_t max_time_;
  uint64_t mean_time_;
  const std::string name_;

  EventStat(const std::string name)
      : last_ts_(0),
        first_ts_(0),
        event_count_(0),
        min_time_(UINT64_MAX),
        max_time_(0),
        mean_time_(0),
        name_(std::move(name)) {
  }

  friend std::ostream &operator<<(std::ostream &out, EventStat &statistic) {
    out << "\ttypeinfo name:" << statistic.name_ << std::endl;
    out << "\t\tlast_ts: " << std::to_string(statistic.last_ts_) << std::endl;
    out << "\t\tfirst_ts: " << std::to_string(statistic.first_ts_)
        << std::endl;
    out << "\t\tevent_count: " << std::to_string(statistic.event_count_)
        << std::endl;
    out << "\t\tmin_time: " << std::to_string(statistic.min_time_)
        << std::endl;
    out << "\t\tmax_time: " << std::to_string(statistic.max_time_)
        << std::endl;
    out << "\t\tmean_time: " << std::to_string(statistic.mean_time_)
        << std::endl;
    return out;
  }
};

/* Filter the event stream by the specified event types and collect some simple
 * statistics about these types */
class EventTypeStatistics : public corobelt::Pipe<std::shared_ptr<Event>> {
  std::set<EventType> types_to_gather_statistic_;

  // TODO: map from event_type,simulator -> statistics
  // NOTE that this is also possible with the pipeline features by gathering
  // statistics before merging event streams of different simulators
  std::map<EventType, std::shared_ptr<EventStat>> statistics_by_type_;

  bool update_statistics(std::shared_ptr<Event> event_ptr) {
    EventType key = event_ptr->getType();
    std::shared_ptr<EventStat> statistic;
    const auto &statistics_search = statistics_by_type_.find(key);
    if (statistics_search == statistics_by_type_.end()) {
      statistic = std::make_shared<EventStat>(event_ptr->getName());
      if (statistic) {
        statistic->first_ts_ = event_ptr->timestamp_;
        statistic->min_time_ = event_ptr->timestamp_;
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

    uint64_t latency = event_ptr->timestamp_ - statistic->last_ts_;
    if (latency < statistic->min_time_) {
      statistic->min_time_ = latency;
    }
    if (latency > statistic->max_time_) {
      statistic->max_time_ = latency;
    }

    statistic->event_count_ = statistic->event_count_ + 1;
    statistic->last_ts_ = event_ptr->timestamp_;

    if (statistic->event_count_ != 0) {
      statistic->mean_time_ = (statistic->last_ts_ - statistic->first_ts_) /
                              statistic->event_count_;
    }
    return true;
  }

 public:
  EventTypeStatistics(std::set<EventType> types_to_gather_statistic)
      : corobelt::Pipe<std::shared_ptr<Event>>(),
        types_to_gather_statistic_(std::move(types_to_gather_statistic)) {
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
    std::shared_ptr<EventStat> statistic_ptr;
    const std::map<EventType, std::shared_ptr<EventStat>> &statistics =
        eventTypeStatistics.getStatistics();
    for (auto it = statistics.begin(); it != statistics.end(); it++) {
      statistic_ptr = it->second;
      EventStat statistic(*statistic_ptr);
      out << statistic;
    }
    return out;
  }

  void process(corobelt::coro_push_t<std::shared_ptr<Event>> &sink,
               corobelt::coro_pull_t<std::shared_ptr<Event>> &source) override {
    for (std::shared_ptr<Event> event_ptr : source) {
      auto search = types_to_gather_statistic_.find(event_ptr->getType());
      if (search != types_to_gather_statistic_.end()) {
        if (!update_statistics(event_ptr)) {
#ifdef DEBUG_EVENT_
          DFLOGWARN("statistics for event with name %s could not be updated\n",
                    event_ptr->getName().c_str());
#endif
        }
      }
      sink(event_ptr);
    }
  }
};

#endif  // SIMBRICKS_TRACE_EVENTS_H_
