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
#include "trace/corobelt/coroutine.h"

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

  virtual ~Event() = default;
};

/* Simbricks Events */
class SimSendSync : public Event {
 public:
  explicit SimSendSync(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::SimSendSync_t, "SimSendSyncSimSendSync") {
  }

  ~SimSendSync() = default;

  void display(std::ostream &os) override;
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::SimProcInEvent_t, "SimProcInEvent") {
  }

  ~SimProcInEvent() = default;

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

  virtual ~HostInstr() = default;

  void display(std::ostream &os) override;
};

class HostCall : public HostInstr {
 public:
  const std::string func_;
  const std::string comp_;

  explicit HostCall(uint64_t ts, LogParser *src, uint64_t pc,
                    const std::string func, const std::string comp)
      : HostInstr(ts, src, pc, EventType::HostCall_t, "HostCall"),
        func_(std::move(func)),
        comp_(std::move(comp)) {
  }

  ~HostCall() = default;

  void display(std::ostream &os) override;
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::HostMmioImRespPoW_t, "HostMmioImRespPoW") {
  }

  ~HostMmioImRespPoW() = default;

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

  virtual ~HostIdOp() = default;
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, EventType::HostMmioCR_t, "HostMmioCR", id) {
  }

  ~HostMmioCR() = default;

  void display(std::ostream &os) override;
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, EventType::HostMmioCW_t, "HostMmioCW", id) {
  }

  ~HostMmioCW() = default;

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
      : HostIdOp(ts, src, type, std::move(name), id), addr_(addr), size_(size) {
  }

  virtual ~HostAddrSizeOp() = default;
};

class HostMmioR : public HostAddrSizeOp {
 public:
  explicit HostMmioR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                     uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostMmioR_t, "HostMmioR", id, addr,
                       size) {
  }

  ~HostMmioR() = default;

  void display(std::ostream &os) override;
};

class HostMmioW : public HostAddrSizeOp {
 public:
  explicit HostMmioW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                     uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostMmioW_t, "HostMmioW", id, addr,
                       size) {
  }

  ~HostMmioW() = default;

  void display(std::ostream &os) override;
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t ts, LogParser *src, uint64_t id)
      : HostIdOp(ts, src, EventType::HostDmaC_t, "HostDmaC", id) {
  }

  ~HostDmaC() = default;

  void display(std::ostream &os) override;
};

class HostDmaR : public HostAddrSizeOp {
 public:
  explicit HostDmaR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostDmaR_t, "HostDmaR", id, addr,
                       size) {
  }

  ~HostDmaR() = default;

  void display(std::ostream &os) override;
};

class HostDmaW : public HostAddrSizeOp {
 public:
  explicit HostDmaW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, src, EventType::HostDmaW_t, "HostDmaW", id, addr,
                       size) {
  }

  ~HostDmaW() = default;

  void display(std::ostream &os) override;
};

class HostMsiX : public Event {
 public:
  uint64_t vec_;

  explicit HostMsiX(uint64_t ts, LogParser *src, uint64_t vec)
      : Event(ts, src, EventType::HostMsiX_t, "HostMsiX"), vec_(vec) {
  }

  ~HostMsiX() = default;

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

  ~HostConf() = default;

  void display(std::ostream &os) override;
};

class HostClearInt : public Event {
 public:
  explicit HostClearInt(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::HostClearInt_t, "HostClearInt") {
  }

  ~HostClearInt() = default;

  void display(std::ostream &os) override;
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t ts, LogParser *src)
      : Event(ts, src, EventType::HostPostInt_t, "HostPostInt") {
  }

  ~HostPostInt() = default;

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

  ~HostPciRW() = default;

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

  ~NicMsix() = default;

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

  virtual ~NicDma() = default;
};

class SetIX : public Event {
 public:
  uint64_t intr_;

  SetIX(uint64_t ts, LogParser *src, uint64_t intr)
      : Event(ts, src, EventType::SetIX_t, "SetIX"), intr_(intr) {
  }

  ~SetIX() = default;

  void display(std::ostream &os) override;
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, src, EventType::NicDmaI_t, "NicDmaI", id, addr, len) {
  }

  ~NicDmaI() = default;

  void display(std::ostream &os) override;
};

class NicDmaEx : public NicDma {
 public:
  NicDmaEx(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaEx_t, "NicDmaEx", id, addr, len) {
  }

  ~NicDmaEx() = default;

  void display(std::ostream &os) override;
};

class NicDmaEn : public NicDma {
 public:
  NicDmaEn(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaEn_t, "NicDmaEn", id, addr, len) {
  }

  ~NicDmaEn() = default;

  void display(std::ostream &os) override;
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaCR_t, "NicDmaCR", id, addr, len) {
  }

  ~NicDmaCR() = default;

  void display(std::ostream &os) override;
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t ts, LogParser *src, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, src, EventType::NicDmaCW_t, "NicDmaCW", id, addr, len) {
  }

  ~NicDmaCW() = default;

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

  virtual ~NicMmio() = default;
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t ts, LogParser *src, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, src, EventType::NicMmioR_t, "NicMmioR", off, len, val) {
  }

  ~NicMmioR() = default;

  void display(std::ostream &os) override;
};

class NicMmioW : public NicMmio {
 public:
  NicMmioW(uint64_t ts, LogParser *src, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, src, EventType::NicMmioW_t, "NicMmioW", off, len, val) {
  }

  ~NicMmioW() = default;

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

  virtual ~NicTrx() = default;
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, LogParser *src, uint16_t len)
      : NicTrx(ts, src, EventType::NicTx_t, "NicTx", len) {
  }

  ~NicTx() = default;

  void display(std::ostream &os) override;
};

class NicRx : public NicTrx {
  uint64_t port_;

 public:
  NicRx(uint64_t ts, LogParser *src, uint64_t port, uint16_t len)
      : NicTrx(ts, src, EventType::NicRx_t, "NicRx", len), port_(port) {
  }

  ~NicRx() = default;

  void display(std::ostream &os) override;
};

inline std::ostream &operator<<(std::ostream &os, Event &e) {
  e.display(os);
  return os;
}

class EventPrinter : public sim::coroutine::consumer<std::shared_ptr<Event>> {
 public:
  sim::coroutine::task<void> consume(
      sim::coroutine::unbuffered_single_chan<std::shared_ptr<Event>> *src_chan)
      override {
    if (!src_chan) {
      co_return;
    }
    std::optional<std::shared_ptr<Event>> msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      std::cout << *(msg.value()) << std::endl;
    }
    co_return;
  }
};

struct EventComperator {
  bool operator()(const std::shared_ptr<Event> &e1,
                  const std::shared_ptr<Event> &e2) const {
    return e1->timestamp_ > e2->timestamp_;
  }
};

bool is_type(std::shared_ptr<Event> event_ptr, EventType type);

bool is_host_issued_mmio_event(std::shared_ptr<Event> event_ptr);

bool is_host_received_mmio_event(std::shared_ptr<Event> event_ptr);

bool is_host_mmio_event(std::shared_ptr<Event> event_ptr);

bool is_host_event(std::shared_ptr<Event> event_ptr);

bool is_nic_event(std::shared_ptr<Event> event_ptr);

#endif  // SIMBRICKS_TRACE_EVENTS_H_
