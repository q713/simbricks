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

inline std::ostream &operator<<(std::stringstream &into, EventType &type) {
  switch (type) {
    case EventType::Event_t:into << "Event_t";
      break;
    case EventType::SimSendSync_t:into << "SimSendSync_t";
      break;
    case EventType::SimProcInEvent_t:into << "SimProcInEvent_t";
      break;
    case EventType::HostInstr_t:into << "HostInstr_t";
      break;
    case EventType::HostCall_t:into << "HostCall_t";
      break;
    case EventType::HostMmioImRespPoW_t:into << "HostMmioImRespPoW_t";
      break;
    case EventType::HostIdOp_t:into << "HostIdOp_t";
      break;
    case EventType::HostMmioCR_t:into << "HostMmioCR_t";
      break;
    case EventType::HostMmioCW_t:into << "HostMmioCW_t";
      break;
    case EventType::HostAddrSizeOp_t:into << "HostAddrSizeOp_t";
      break;
    case EventType::HostMmioR_t:into << "HostMmioR_t";
      break;
    case EventType::HostMmioW_t:into << "HostMmioW_t";
      break;
    case EventType::HostDmaC_t:into << "HostDmaC_t";
      break;
    case EventType::HostDmaR_t:into << "HostDmaR_t";
      break;
    case EventType::HostDmaW_t:into << "HostDmaW_t";
      break;
    case EventType::HostMsiX_t:into << "HostMsiX_t";
      break;
    case EventType::HostConf_t:into << "HostConf_t";
      break;
    case EventType::HostClearInt_t:into << "HostClearInt_t";
      break;
    case EventType::HostPostInt_t:into << "HostPostInt_t";
      break;
    case EventType::HostPciRW_t:into << "HostPciRW_t";
      break;
    case EventType::NicMsix_t:into << "NicMsix_t";
      break;
    case EventType::NicDma_t:into << "NicDma_t";
      break;
    case EventType::SetIX_t:into << "SetIX_t";
      break;
    case EventType::NicDmaI_t:into << "NicDmaI_t";
      break;
    case EventType::NicDmaEx_t:into << "NicDmaEx_t";
      break;
    case EventType::NicDmaEn_t:into << "NicDmaEn_t";
      break;
    case EventType::NicDmaCR_t:into << "NicDmaCR_t";
      break;
    case EventType::NicDmaCW_t:into << "NicDmaCW_t";
      break;
    case EventType::NicMmio_t:into << "NicMmio_t";
      break;
    case EventType::NicMmioR_t:into << "NicMmioR_t";
      break;
    case EventType::NicMmioW_t:into << "NicMmioW_t";
      break;
    case EventType::NicTrx_t:into << "NicTrx_t";
      break;
    case EventType::NicTx_t:into << "NicTx_t";
      break;
    case EventType::NicRx_t:into << "NicRx_t";
      break;
    default:throw_just("encountered unknown event type");
  }
  return into;
}

/* Parent class for all events of interest */
class Event {
  EventType type_;
  std::string name_;

 public:
  uint64_t timestamp_;
  const size_t parser_identifier_;
  // TODO: optimize string name out in later versions
  const std::string parser_name_;

  inline size_t get_parser_ident() const {
    return parser_identifier_;
  }

  inline const std::string &get_name() {
    return name_;
  }

  inline const std::string &get_parser_name() {
    return parser_name_;
  }

  EventType get_type() const {
    return type_;
  }

  inline uint64_t get_ts() const {
    return timestamp_;
  }

  virtual void display(std::ostream &out);

  virtual bool equal(const Event &other);

  virtual ~Event() = default;

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

  void display(std::ostream &out) override;

  virtual bool equal(const Event &other) override;
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t ts, const size_t parser_identifier,
                          const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::SimProcInEvent_t, "SimProcInEvent") {
  }

  ~SimProcInEvent() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  uint64_t GetPc() const;

  virtual ~HostInstr() = default;

  void display(std::ostream &out) override;

  virtual bool equal(const Event &other) override;
};

class HostCall : public HostInstr {
 public:
  const std::string *func_;
  const std::string *comp_;

  explicit HostCall(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t pc,
                    const std::string *func,
                    const std::string *comp)
      : HostInstr(ts, parser_identifier, parser_name, pc,
                  EventType::HostCall_t, "HostCall"),
        func_(func),
        comp_(comp) {
  }

  ~HostCall() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  const std::string *GetFunc() const;

  const std::string *GetComp() const;
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t ts, const size_t parser_identifier,
                             const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostMmioImRespPoW_t, "HostMmioImRespPoW") {
  }

  ~HostMmioImRespPoW() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};

class HostIdOp : public Event {
 public:
  uint64_t id_;

  void display(std::ostream &out) override;

  uint64_t GetId() const;

 protected:
  explicit HostIdOp(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, EventType type,
                    std::string name, uint64_t id)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        id_(id) {
  }

  virtual ~HostIdOp() = default;

  virtual bool equal(const Event &other) override;
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::HostMmioCR_t, "HostMmioCR", id) {
  }

  ~HostMmioCR() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::HostMmioCW_t, "HostMmioCW", id) {
  }

  ~HostMmioCW() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};

class HostAddrSizeOp : public HostIdOp {
 public:
  uint64_t addr_;
  uint64_t size_;

  void display(std::ostream &out) override;

  uint64_t GetAddr() const;

  uint64_t GetSize() const;

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

  virtual bool equal(const Event &other) override;
};

class HostMmioOp : public HostAddrSizeOp {
 public:
  uint64_t bar_;
  uint64_t offset_;

  void display(std::ostream &out) override;

  uint64_t GetBar() const;

  uint64_t GetOffset() const;

 protected:
  explicit HostMmioOp(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, EventType type,
                      std::string name, uint64_t id, uint64_t addr,
                      uint64_t size, uint64_t bar, uint64_t offset)
      : HostAddrSizeOp(ts, parser_identifier, parser_name, type,
                       std::move(name), id, addr, size), bar_(bar), offset_(offset) {
  }

  virtual bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::HostDmaC_t, "HostDmaC", id) {
  }

  ~HostDmaC() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  uint64_t GetVec() const;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  uint64_t GetDev() const;

  uint64_t GetFunc() const;

  uint64_t GetReg() const;

  uint64_t GetBytes() const;

  uint64_t GetData() const;

  bool IsRead() const;
};

class HostClearInt : public Event {
 public:
  explicit HostClearInt(uint64_t ts, const size_t parser_identifier,
                        const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostClearInt_t, "HostClearInt") {
  }

  ~HostClearInt() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t ts, const size_t parser_identifier,
                       const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::HostPostInt_t, "HostPostInt") {
  }

  ~HostPostInt() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  uint64_t GetOffset() const;

  uint64_t GetSize() const;

  bool IsRead() const;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  uint16_t GetVec() const;

  bool IsX() const;
};

class NicDma : public Event {
 public:
  uint64_t id_;
  uint64_t addr_;
  uint64_t len_;

  void display(std::ostream &out) override;

  uint64_t GetId() const;

  uint64_t GetAddr() const;

  uint64_t GetLen() const;

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

  virtual bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  uint64_t GetIntr() const;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};

class NicMmio : public Event {
 public:
  uint64_t off_;
  uint64_t len_;
  uint64_t val_;

  virtual void display(std::ostream &out) override;

  uint64_t GetOff() const;

  uint64_t GetLen() const;

  uint64_t GetVal() const;

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

  virtual bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
};

class NicTrx : public Event {
 public:
  uint16_t len_;

  void display(std::ostream &out) override;

  uint16_t GetLen() const;

 protected:
  NicTrx(uint64_t ts, const size_t parser_identifier,
         const std::string parser_name, EventType type, std::string name,
         uint16_t len)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        len_(len) {
  }

  virtual ~NicTrx() = default;

  virtual bool equal(const Event &other) override;
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, uint16_t len)
      : NicTrx(ts, parser_identifier, parser_name,
               EventType::NicTx_t, "NicTx", len) {
  }

  ~NicTx() = default;

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;
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

  void display(std::ostream &out) override;

  bool equal(const Event &other) override;

  uint64_t GetPort() const;
};

inline std::ostream &operator<<(std::ostream &out, Event &e) {
  e.display(out);
  return out;
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
         msg = co_await src_chan->pop(resume_executor)) {
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

bool is_type(std::shared_ptr<Event> &event_ptr, EventType type);

bool is_type(const Event &event, EventType type);

#endif  // SIMBRICKS_TRACE_EVENTS_H_
