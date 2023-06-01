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

#ifndef SIMBRICKS_TRACE_EVENTS_H_
#define SIMBRICKS_TRACE_EVENTS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>

#include "util/exception.h"
#include "corobelt/corobelt.h"
#include "util/log.h"

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
  const size_t parser_identifier_;
  // TODO: optimize string name out in later versions
  const std::string parser_name_;

  inline size_t get_parser_ident() {
    return parser_identifier_;
  }

  inline const std::string &get_name() {
    return name_;
  }

  EventType get_type() {
    return type_;
  }

  inline uint64_t get_ts() {
    return timestamp_;
  }

  virtual void display(std::ostream &os);

 protected:
  explicit Event(uint64_t ts, const size_t parser_identifier,
                 const std::string parser_name, EventType type,
                 std::string name)
      : type_(type),
        name_(std::move(name)),
        timestamp_(ts),
        parser_identifier_(parser_identifier),
        parser_name_(parser_name) {
  }

  virtual ~Event() = default;
};

/* Simbricks Events */
class SimSendSync : public Event {
 public:
  explicit SimSendSync(uint64_t ts, const size_t parser_identifier,
                       const std::string parser_name)
      : Event(ts, parser_identifier, parser_name, EventType::SimSendSync_t,
              "SimSendSyncSimSendSync") {
  }

  ~SimSendSync() = default;

  void display(std::ostream &os) override;
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t ts, const size_t parser_identifier,
                          const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::SimProcInEvent_t, "SimProcInEvent") {
  }

  ~SimProcInEvent() = default;

  void display(std::ostream &os) override;
};

/* Host related events */

class HostInstr : public Event {
 public:
  uint64_t pc_;

  HostInstr(uint64_t ts, const size_t parser_identifier,
            const std::string parser_name, uint64_t pc)
      : Event(ts, parser_identifier, std::move(parser_name),
              EventType::HostInstr_t, "HostInstr"),
        pc_(pc) {
  }

  HostInstr(uint64_t ts, size_t parser_identifier, std::string parser_name,
            uint64_t pc, EventType type, std::string name)
      : Event(ts, parser_identifier, std::move(parser_name), type, name),
        pc_(pc) {
  }

  virtual ~HostInstr() = default;

  void display(std::ostream &os) override;
};

class HostCall : public HostInstr {
 public:
  const std::string * func_;
  const std::string *comp_;

  explicit HostCall(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t pc,
                    const std::string * func,
                    const std::string *comp)
      : HostInstr(ts, parser_identifier, parser_name, pc,
                  EventType::HostCall_t, "HostCall"),
        func_(func),
        comp_(std::move(comp)) {
  }

  ~HostCall() = default;

  void display(std::ostream &os) override;
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t ts, const size_t parser_identifier,
                             const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostMmioImRespPoW_t, "HostMmioImRespPoW") {
  }

  ~HostMmioImRespPoW() = default;

  void display(std::ostream &os) override;
};

class HostIdOp : public Event {
 public:
  uint64_t id_;

  void display(std::ostream &os) override;

 protected:
  explicit HostIdOp(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, EventType type,
                    std::string name, uint64_t id)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        id_(id) {
  }

  virtual ~HostIdOp() = default;
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::HostMmioCR_t, "HostMmioCR", id) {
  }

  ~HostMmioCR() = default;

  void display(std::ostream &os) override;
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::HostMmioCW_t, "HostMmioCW", id) {
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
  explicit HostAddrSizeOp(uint64_t ts, const size_t parser_identifier,
                          const std::string parser_name, EventType type,
                          std::string name, uint64_t id, uint64_t addr,
                          uint64_t size)
      : HostIdOp(ts, parser_identifier, parser_name, type,
                 std::move(name), id),
        addr_(addr),
        size_(size) {
  }

  virtual ~HostAddrSizeOp() = default;
};

class HostMmioOp : public HostAddrSizeOp {
 public:
  uint64_t bar_;
  uint64_t offset_;

  void display(std::ostream &os) override;

 protected:
  explicit HostMmioOp(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, EventType type,
                      std::string name, uint64_t id, uint64_t addr,
                      uint64_t size, uint64_t bar, uint64_t offset)
      : HostAddrSizeOp(ts, parser_identifier, parser_name, type,
                       std::move(name), id, addr, size) {
  }
};

class HostMmioR : public HostMmioOp {
 public:
  explicit HostMmioR(uint64_t ts, const size_t parser_identifier,
                     const std::string parser_name, uint64_t id, uint64_t addr,
                     uint64_t size, uint64_t bar, uint64_t offset)
      : HostMmioOp(ts, parser_identifier, parser_name,
                   EventType::HostMmioR_t, "HostMmioR", id, addr, size, bar,
                   offset) {
  }

  ~HostMmioR() = default;

  void display(std::ostream &os) override;
};

class HostMmioW : public HostMmioOp {
 public:
  explicit HostMmioW(uint64_t ts, const size_t parser_identifier,
                     const std::string parser_name, uint64_t id, uint64_t addr,
                     uint64_t size, uint64_t bar, uint64_t offset)
      : HostMmioOp(ts, parser_identifier, parser_name,
                   EventType::HostMmioW_t, "HostMmioW", id, addr, size, bar,
                   offset) {
  }

  ~HostMmioW() = default;

  void display(std::ostream &os) override;
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::HostDmaC_t, "HostDmaC", id) {
  }

  ~HostDmaC() = default;

  void display(std::ostream &os) override;
};

class HostDmaR : public HostAddrSizeOp {
 public:
  explicit HostDmaR(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, parser_identifier, parser_name,
                       EventType::HostDmaR_t, "HostDmaR", id, addr, size) {
  }

  ~HostDmaR() = default;

  void display(std::ostream &os) override;
};

class HostDmaW : public HostAddrSizeOp {
 public:
  explicit HostDmaW(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id, uint64_t addr,
                    uint64_t size)
      : HostAddrSizeOp(ts, parser_identifier, parser_name,
                       EventType::HostDmaW_t, "HostDmaW", id, addr, size) {
  }

  ~HostDmaW() = default;

  void display(std::ostream &os) override;
};

class HostMsiX : public Event {
 public:
  uint64_t vec_;

  explicit HostMsiX(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t vec)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostMsiX_t, "HostMsiX"),
        vec_(vec) {
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

  explicit HostConf(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t dev, uint64_t func,
                    uint64_t reg, uint64_t bytes, uint64_t data, bool is_read)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostConf_t,
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
  explicit HostClearInt(uint64_t ts, const size_t parser_identifier,
                        const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostClearInt_t, "HostClearInt") {
  }

  ~HostClearInt() = default;

  void display(std::ostream &os) override;
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t ts, const size_t parser_identifier,
                       const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostPostInt_t, "HostPostInt") {
  }

  ~HostPostInt() = default;

  void display(std::ostream &os) override;
};

class HostPciRW : public Event {
 public:
  uint64_t offset_;
  uint64_t size_;
  bool is_read_;

  explicit HostPciRW(uint64_t ts, const size_t parser_identifier,
                     const std::string parser_name, uint64_t offset,
                     uint64_t size, bool is_read)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostPciRW_t, is_read ? "HostPciR" : "HostPciW"),
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

  NicMsix(uint64_t ts, const size_t parser_identifier,
          const std::string parser_name, uint16_t vec, bool isX)
      : Event(ts, parser_identifier, parser_name,
              EventType::NicMsix_t, isX ? "NicMsix" : "NicMsi"),
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
  NicDma(uint64_t ts, const size_t parser_identifier,
         const std::string parser_name, EventType type, std::string name,
         uint64_t id, uint64_t addr, uint64_t len)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        id_(id),
        addr_(addr),
        len_(len) {
  }

  virtual ~NicDma() = default;
};

class SetIX : public Event {
 public:
  uint64_t intr_;

  SetIX(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, uint64_t intr)
      : Event(ts, parser_identifier, parser_name, EventType::SetIX_t,
              "SetIX"),
        intr_(intr) {
  }

  ~SetIX() = default;

  void display(std::ostream &os) override;
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t ts, const size_t parser_identifier,
          const std::string parser_name, uint64_t id, uint64_t addr,
          uint64_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::NicDmaI_t, "NicDmaI", id, addr, len) {
  }

  ~NicDmaI() = default;

  void display(std::ostream &os) override;
};

class NicDmaEx : public NicDma {
 public:
  NicDmaEx(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::NicDmaEx_t, "NicDmaEx", id, addr, len) {
  }

  ~NicDmaEx() = default;

  void display(std::ostream &os) override;
};

class NicDmaEn : public NicDma {
 public:
  NicDmaEn(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::NicDmaEn_t, "NicDmaEn", id, addr, len) {
  }

  ~NicDmaEn() = default;

  void display(std::ostream &os) override;
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::NicDmaCR_t, "NicDmaCR", id, addr, len) {
  }

  ~NicDmaCR() = default;

  void display(std::ostream &os) override;
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           uint64_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::NicDmaCW_t, "NicDmaCW", id, addr, len) {
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
  NicMmio(uint64_t ts, const size_t parser_identifier,
          const std::string parser_name, EventType type, std::string name,
          uint64_t off, uint64_t len, uint64_t val)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        off_(off),
        len_(len),
        val_(val) {
  }

  virtual ~NicMmio() = default;
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, parser_identifier, parser_name,
                EventType::NicMmioR_t, "NicMmioR", off, len, val) {
  }

  ~NicMmioR() = default;

  void display(std::ostream &os) override;
};

class NicMmioW : public NicMmio {
 public:
  NicMmioW(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t off, uint64_t len,
           uint64_t val)
      : NicMmio(ts, parser_identifier, parser_name,
                EventType::NicMmioW_t, "NicMmioW", off, len, val) {
  }

  ~NicMmioW() = default;

  void display(std::ostream &os) override;
};

class NicTrx : public Event {
 public:
  uint16_t len_;

  void display(std::ostream &os) override;

 protected:
  NicTrx(uint64_t ts, const size_t parser_identifier,
         const std::string parser_name, EventType type, std::string name,
         uint16_t len)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        len_(len) {
  }

  virtual ~NicTrx() = default;
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, uint16_t len)
      : NicTrx(ts, parser_identifier, parser_name,
               EventType::NicTx_t, "NicTx", len) {
  }

  ~NicTx() = default;

  void display(std::ostream &os) override;
};

class NicRx : public NicTrx {
  uint64_t port_;

 public:
  NicRx(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, uint64_t port, uint16_t len)
      : NicTrx(ts, parser_identifier, parser_name,
               EventType::NicRx_t, "NicRx", len),
        port_(port) {
  }

  ~NicRx() = default;

  void display(std::ostream &os) override;
};

inline std::ostream &operator<<(std::ostream &os, Event &e) {
  e.display(os);
  return os;
}

class EventPrinter : public consumer<std::shared_ptr<Event>> {
  std::ostream &out_;

 public:

  explicit EventPrinter(std::ostream &out) : out_(out) {
  }

  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan
      ) override {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(src_chan, channel_is_null);

    std::shared_ptr<Event> event;
    std::optional<std::shared_ptr<Event>> msg;
    for (msg = co_await src_chan->pop(resume_executor); msg.has_value();
         msg = co_await  src_chan->pop(resume_executor)) {
      event = msg.value();
      throw_if_empty(event, event_is_null);
      out_ << *event << std::endl;
    }
    co_return;
  }
};

struct EventComperator {
  bool operator()(const std::shared_ptr<Event> &ev1,
                  const std::shared_ptr<Event> &ev2) const {
    return ev1->timestamp_ > ev2->timestamp_;
  }
};

bool is_type(std::shared_ptr<Event> event_ptr, EventType type);

#endif  // SIMBRICKS_TRACE_EVENTS_H_
