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
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

#include "trace/corobelt/belt.h"
#include "trace/parser/parser.h"

/* Parent class for all events of interest */
class Event {
 public:
  uint64_t timestamp_;
  LogParser *src_;

  explicit Event(uint64_t ts, LogParser *src) : timestamp_(ts), src_(src) {
  }

  virtual void display(std::ostream &os) {
    os << "Event: source=" << src_->getIdent() << ", timestamp=" << timestamp_
       << " ";
  }
};

/* Simbricks Events */
class SimSendSync : public Event {
 public:
  explicit SimSendSync(uint64_t ts, LogParser *src) : Event(ts, src) {
  }

  void display(std::ostream &os) override {
    os << "simbricks: sending sync message ";
    Event::display(os);
  }
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t ts, LogParser *src) : Event(ts, src) {
  }

  void display(std::ostream &os) override {
    os << "simbricks: processInEvent ";
    Event::display(os);
  }
};

/* Host related events */
class HostCall : public Event {
 public:
  const std::string func_;

  HostCall(uint64_t ts, LogParser *src, const std::string func)
      : Event(ts, src), func_(std::move(func)) {
  }

  void display(std::ostream &os) override {
    os << "H.CALL ";
    Event::display(os);
    os << "func=" << func_;
  }
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t ts, LogParser *src) : Event(ts, src) {
  }

  void display(std::ostream &os) override {
    os << "HostMmioImRespPoW ";
    Event::display(os);
  }
};

class HostIdOp : public Event {
  uint64_t id_;

 public:
  explicit HostIdOp(uint64_t ts, LogParser *src, uint64_t id)
      : Event(ts, src), id_(id) {
  }

  void display(std::ostream &os) override {
    Event::display(os);
    os << "id=" << id_;
  }
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, id) {
  }

  void display(std::ostream &os) override {
    os << "HostMmioCR ";
    HostIdOp::display(os);
  }
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, id) {
  }

  void display(std::ostream &os) override {
    os << "HostMmioCW ";
    HostIdOp::display(os);
  }
};

class HostAddrSizeOp : public HostIdOp {
  uint64_t addr_;
  uint64_t size_;

 public:
  explicit HostAddrSizeOp(uint64_t ts, LogParser *src, uint64_t id,
                          uint64_t addr, uint64_t size)
      : HostIdOp(ts, src, id) {
  }

  void display(std::ostream &os) override {
    HostIdOp::display(os);
    os << ", addr=" << std::hex << addr_ << ", size=" << size_ << " ";
  }
};

class HostMmioR : public HostAddrSizeOp {
 public:
  explicit HostMmioR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                     uint64_t size)
      : HostAddrSizeOp(ts, src, id, addr, size) {
  }

  void display(std::ostream &os) override {
    os << "HostMmioR ";
    HostAddrSizeOp::display(os);
  }
};

class HostMmioW : public HostAddrSizeOp {
 public:
  explicit HostMmioW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                     uint64_t size)
      : HostAddrSizeOp(ts, src, id, addr, size) {
  }

  void display(std::ostream &os) override {
    os << "HostMmioW ";
    HostAddrSizeOp::display(os);
  }
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, id) {
  }

  void display(std::ostream &os) override {
    os << "HostDmaC ";
    HostIdOp::display(os);
  }
};

class HostDmaR : public HostAddrSizeOp {
 public:
  explicit HostDmaR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, src, id, addr, size) {
  }

  void display(std::ostream &os) override {
    os << "HostDmaR ";
    HostAddrSizeOp::display(os);
  }
};

class HostDmaW : public HostAddrSizeOp {
 public:
  explicit HostDmaW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, src, id, addr, size) {
  }

  void display(std::ostream &os) override {
    os << "HostDmaW ";
    HostAddrSizeOp::display(os);
  }
};

class HostMsiX : public Event {
  uint64_t vec_;

 public:
  explicit HostMsiX(uint64_t ts, LogParser *src, uint64_t vec)
      : Event(ts, src), vec_(vec) {
  }

  void display(std::ostream &os) override {
    os << "HostMsiX ";
    Event::display(os);
    os << ", vec=" << vec_;
  }
};

class HostConf : public Event {
  uint64_t dev_;
  uint64_t func_;
  uint64_t reg_;
  uint64_t bytes_;
  uint64_t data_;
  bool is_read_;

 public:
  explicit HostConf(uint64_t ts, LogParser *src, uint64_t dev, uint64_t func,
                    uint64_t reg, uint64_t bytes, uint64_t data, bool is_read)
      : Event(ts, src),
        dev_(dev),
        func_(func),
        reg_(reg),
        bytes_(bytes),
        data_(data),
        is_read_(is_read) {
  }

  void display(std::ostream &os) override {
    if (is_read_) {
      os << "HostConfRead ";
    } else {
      os << "HostConfWrite ";
    }
    Event::display(os);
    os << ", dev=" << dev_ << ", func=" << func_ << ", reg=" << std::hex << reg_
       << ", bytes=" << bytes_ << ", data=" << std::hex << data_;
  }
};

class HostClearInt : public Event {
 public:
  explicit HostClearInt(uint64_t ts, LogParser *src) : Event(ts, src) {
  }

  void display(std::ostream &os) override {
    os << "HostClearInt ";
    Event::display(os);
  }
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t ts, LogParser *src) : Event(ts, src) {
  }

  void display(std::ostream &os) override {
    os << "HostPostInt ";
    Event::display(os);
  }
};

class HostPciR : public Event {
  uint64_t offset_;
  uint64_t size_;

 public:
  explicit HostPciR(uint64_t ts, LogParser *src, uint64_t offset, uint64_t size)
      : Event(ts, src), offset_(offset), size_(size) {
  }

  void display(std::ostream &os) override {
    os << "HostPciR ";
    Event::display(os);
    os << ", offset=" << std::hex << offset_ << ", size=" << std::hex << size_;
  }
};

/* NIC related events */
class NicMsix : public Event {
 public:
  uint16_t vec_;
  bool isX_;

  NicMsix(uint64_t ts, LogParser *src, uint16_t vec, bool isX)
      : Event(ts, src), vec_(vec), isX_(isX) {
  }

  void display(std::ostream &os) override {
    if (isX_) {
      os << "N.MSIX ";
    } else {
      os << "N.MSI ";
    }
    Event::display(os);
    os << ", vec=" << vec_;
  }
};

class NicDma : public Event {
 public:
  uint64_t id_;
  uint64_t addr_;
  uint64_t len_;

  NicDma(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr, uint64_t len)
      : Event(ts, src), id_(id), addr_(addr), len_(len) {
  }

  void display(std::ostream &os) override {
    Event::display(os);
    os << "id=" << std::hex << id_ << ", addr=" << std::hex << addr_
       << ", size=" << len_;
  }
};

class SetIX : public Event {
 public:
  uint64_t intr_;

  SetIX(uint64_t ts, LogParser *src, uint64_t intr)
      : Event(ts, src), intr_(intr) {
  }

  void display(std::ostream &os) override {
    os << "N.SETIX ";
    Event::display(os);
    os << "interrupt=" << std::hex << intr_;
  }
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, src, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMAI ";
    NicDma::display(os);
  }
};

class NicDmaEx : public NicDma {
 public:
  NicDmaEx(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMAEX ";
    NicDma::display(os);
  }
};

class NicDmaEn : public NicDma {
 public:
  NicDmaEn(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMAEN ";
    NicDma::display(os);
  }
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMACR ";
    NicDma::display(os);
  }
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMACW ";
    NicDma::display(os);
  }
};

class NicMmio : public Event {
 public:
  uint64_t off_;
  uint64_t len_;
  uint64_t val_;

  NicMmio(uint64_t ts, LogParser *src, uint64_t off, uint64_t len, uint64_t val)
      : Event(ts, src), off_(off), len_(len), val_(val) {
  }

  virtual void display(std::ostream &os) override {
    Event::display(os);
    os << "off=" << std::hex << off_ << ", len=" << len_ << " val=" << std::hex
       << val_;
  }
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t ts, LogParser *src, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, src, off, len, val) {
  }

  void display(std::ostream &os) override {
    os << "N.MMIOR ";
    NicMmio::display(os);
  }
};

class NicMmioW : public NicMmio {
 public:
  NicMmioW(uint64_t ts, LogParser *src, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, src, off, len, val) {
  }

  void display(std::ostream &os) override {
    os << "N.MMIOW ";
    NicMmio::display(os);
  }
};

class NicTrx : public Event {
 public:
  uint16_t len_;

  NicTrx(uint64_t ts, LogParser *src, uint16_t len)
      : Event(ts, src), len_(len) {
  }

  void display(std::ostream &os) override {
    Event::display(os);
    os << ", len=" << len_;
  }
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, LogParser *src, uint16_t len) : NicTrx(ts, src, len) {
  }

  void display(std::ostream &os) override {
    os << "N.TX ";
    NicTrx::display(os);
  }
};

class NicRx : public NicTrx {
  uint64_t port_;

 public:
  NicRx(uint64_t ts, LogParser *src, uint64_t port, uint16_t len)
      : NicTrx(ts, src, len), port_(port) {
  }

  void display(std::ostream &os) override {
    os << "N.RX ";
    NicTrx::display(os);
    os << ", port=" << port_;
  }
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

template <typename... EventTypes>
class EventTypeFilter : public corobelt::Pipe<std::shared_ptr<Event>> {
 private:
  template <typename event_type_a, typename event_type_b,
            typename... event_types>
  bool is_one_of(std::shared_ptr<Event> event) {
    bool is = false;
    is = is || is_one_of<event_type_a>(event);
    is = is || is_one_of<event_type_b, event_types...>(event);
    return is;
  }

  template <typename event_type>
  bool is_one_of(std::shared_ptr<Event> event) {
    // maybe it might be better to use an enum type which is embeded into the
    // events
    std::shared_ptr<event_type> e =
        std::dynamic_pointer_cast<event_type>(event);
    if (e) {
      return true;
    }
    return false;
  }

 public:
  explicit EventTypeFilter() : corobelt::Pipe<std::shared_ptr<Event>>() {
    static_assert(
        std::conjunction<std::is_base_of<Event, EventTypes>...>::value,
        "the type given in the template argument is not a subclass of Event");
  }

  void process(corobelt::coro_push_t<std::shared_ptr<Event>> &sink,
               corobelt::coro_pull_t<std::shared_ptr<Event>> &source) override {
    for (std::shared_ptr<Event> event : source) {
      bool isToSink = is_one_of<EventTypes...>(event);
      if (isToSink) {
        sink(event);
      }
    }
  }
};

#endif  // SIMBRICKS_TRACE_EVENTS_H_
