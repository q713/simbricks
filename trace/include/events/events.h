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
#include "sync/corobelt.h"
#include "util/log.h"

#define DEBUG_EVENT_ ;

class LogParser;

enum EventType {
  kEventT,
  kSimSendSyncT,
  kSimProcInEventT,
  kHostInstrT,
  kHostCallT,
  kHostMmioImRespPoWT,
  kHostIdOpT,
  kHostMmioCRT,
  kHostMmioCWT,
  kHostAddrSizeOpT,
  kHostMmioRT,
  kHostMmioWT,
  kHostDmaCT,
  kHostDmaRT,
  kHostDmaWT,
  kHostMsiXT,
  kHostConfT,
  kHostClearIntT,
  kHostPostIntT,
  kHostPciRWT,
  kNicMsixT,
  kNicDmaT,
  kSetIXT,
  kNicDmaIT,
  kNicDmaExT,
  kNicDmaEnT,
  kNicDmaCRT,
  kNicDmaCWT,
  kNicMmioT,
  kNicMmioRT,
  kNicMmioWT,
  kNicTrxT,
  kNicTxT,
  kNicRxT
};

inline std::ostream &operator<<(std::stringstream &into, EventType type) {
  switch (type) {
    case EventType::kEventT:into << "kEventT";
      break;
    case EventType::kSimSendSyncT:into << "kSimSendSyncT";
      break;
    case EventType::kSimProcInEventT:into << "kSimProcInEventT";
      break;
    case EventType::kHostInstrT:into << "kHostInstrT";
      break;
    case EventType::kHostCallT:into << "kHostCallT";
      break;
    case EventType::kHostMmioImRespPoWT:into << "kHostMmioImRespPoWT";
      break;
    case EventType::kHostIdOpT:into << "kHostIdOpT";
      break;
    case EventType::kHostMmioCRT:into << "kHostMmioCRT";
      break;
    case EventType::kHostMmioCWT:into << "kHostMmioCWT";
      break;
    case EventType::kHostAddrSizeOpT:into << "kHostAddrSizeOpT";
      break;
    case EventType::kHostMmioRT:into << "kHostMmioRT";
      break;
    case EventType::kHostMmioWT:into << "kHostMmioWT";
      break;
    case EventType::kHostDmaCT:into << "kHostDmaCT";
      break;
    case EventType::kHostDmaRT:into << "kHostDmaRT";
      break;
    case EventType::kHostDmaWT:into << "kHostDmaWT";
      break;
    case EventType::kHostMsiXT:into << "kHostMsiXT";
      break;
    case EventType::kHostConfT:into << "kHostConfT";
      break;
    case EventType::kHostClearIntT:into << "kHostClearIntT";
      break;
    case EventType::kHostPostIntT:into << "kHostPostIntT";
      break;
    case EventType::kHostPciRWT:into << "kHostPciRWT";
      break;
    case EventType::kNicMsixT:into << "kNicMsixT";
      break;
    case EventType::kNicDmaT:into << "kNicDmaT";
      break;
    case EventType::kSetIXT:into << "kSetIXT";
      break;
    case EventType::kNicDmaIT:into << "kNicDmaIT";
      break;
    case EventType::kNicDmaExT:into << "kNicDmaExT";
      break;
    case EventType::kNicDmaEnT:into << "kNicDmaEnT";
      break;
    case EventType::kNicDmaCRT:into << "kNicDmaCRT";
      break;
    case EventType::kNicDmaCWT:into << "kNicDmaCWT";
      break;
    case EventType::kNicMmioT:into << "kNicMmioT";
      break;
    case EventType::kNicMmioRT:into << "kNicMmioRT";
      break;
    case EventType::kNicMmioWT:into << "kNicMmioWT";
      break;
    case EventType::kNicTrxT:into << "kNicTrxT";
      break;
    case EventType::kNicTxT:into << "kNicTxT";
      break;
    case EventType::kNicRxT:into << "kNicRxT";
      break;
    default:throw_just("encountered unknown event type");
  }
  return into;
}

/* Parent class for all events of interest */
class Event {
  EventType type_;
  std::string name_;
  uint64_t timestamp_;
  const size_t parser_identifier_;
  // TODO: optimize string name out in later versions
  const std::string parser_name_;

 public:
  inline size_t GetParserIdent() const {
    return parser_identifier_;
  }

  inline const std::string &GetName() {
    return name_;
  }

  inline const std::string &GetParserName() {
    return parser_name_;
  }

  EventType GetType() const {
    return type_;
  }

  inline uint64_t GetTs() const {
    return timestamp_;
  }

  virtual void Display(std::ostream &out);

  virtual bool Equal(const Event &other);

  virtual Event *clone() {
    return new Event(*this);
  }

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

  Event(const Event &other) = default;
};

/* Simbricks Events */
class SimSendSync : public Event {
 public:
  explicit SimSendSync(uint64_t ts, const size_t parser_identifier,
                       const std::string parser_name)
      : Event(ts, parser_identifier, parser_name, EventType::kSimSendSyncT,
              "SimSendSyncSimSendSync") {
  }

  SimSendSync(const SimSendSync &other) = default;

  Event *clone() override {
    return new SimSendSync(*this);
  }

  ~SimSendSync() override = default;

  void Display(std::ostream &out) override;

  virtual bool Equal(const Event &other) override;
};

class SimProcInEvent : public Event {
 public:
  explicit SimProcInEvent(uint64_t ts, const size_t parser_identifier,
                          const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::kSimProcInEventT, "SimProcInEvent") {
  }

  SimProcInEvent(const SimProcInEvent &other) = default;

  Event *clone() override {
    return new SimProcInEvent(*this);
  }

  ~SimProcInEvent() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

/* Host related events */

class HostInstr : public Event {

  uint64_t pc_;

 public:
  HostInstr(uint64_t ts, const size_t parser_identifier,
            const std::string parser_name, uint64_t pc)
      : Event(ts, parser_identifier, std::move(parser_name),
              EventType::kHostInstrT, "HostInstr"),
        pc_(pc) {
  }

  HostInstr(uint64_t ts, size_t parser_identifier, std::string parser_name,
            uint64_t pc, EventType type, std::string name)
      : Event(ts, parser_identifier, std::move(parser_name), type, name),
        pc_(pc) {
  }

  HostInstr(const HostInstr &other) = default;

  Event *clone() override {
    return new HostInstr(*this);
  }

  uint64_t GetPc() const;

  ~HostInstr() override = default;

  void Display(std::ostream &out) override;

  virtual bool Equal(const Event &other) override;
};

class HostCall : public HostInstr {

  const std::string *func_;
  const std::string *comp_;

 public:
  explicit HostCall(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t pc,
                    const std::string *func,
                    const std::string *comp)
      : HostInstr(ts, parser_identifier, parser_name, pc,
                  EventType::kHostCallT, "HostCall"),
        func_(func),
        comp_(comp) {
  }

  HostCall(const HostCall &other) = default;

  Event *clone() override {
    return new HostCall(*this);
  }

  ~HostCall() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  const std::string *GetFunc() const;

  const std::string *GetComp() const;
};

class HostMmioImRespPoW : public Event {
 public:
  explicit HostMmioImRespPoW(uint64_t ts, const size_t parser_identifier,
                             const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::kHostMmioImRespPoWT, "HostMmioImRespPoW") {
  }

  HostMmioImRespPoW(const HostMmioImRespPoW &other) = default;

  Event *clone() override {
    return new HostMmioImRespPoW(*this);
  }

  ~HostMmioImRespPoW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostIdOp : public Event {
  uint64_t id_;

 public:
  void Display(std::ostream &out) override;

  uint64_t GetId() const;

  Event *clone() override {
    return new HostIdOp(*this);
  }

 protected:
  explicit HostIdOp(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, EventType type,
                    std::string name, uint64_t id)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        id_(id) {
  }

  HostIdOp(const HostIdOp &other) = default;

  ~HostIdOp() override = default;

  virtual bool Equal(const Event &other) override;
};

class HostMmioCR : public HostIdOp {
 public:
  explicit HostMmioCR(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::kHostMmioCRT, "HostMmioCR", id) {
  }

  HostMmioCR(const HostMmioCR &other) = default;

  Event *clone() override {
    return new HostMmioCR(*this);
  }

  ~HostMmioCR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};
class HostMmioCW : public HostIdOp {
 public:
  explicit HostMmioCW(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::kHostMmioCWT, "HostMmioCW", id) {
  }

  HostMmioCW(const HostMmioCW &other) = default;

  Event *clone() override {
    return new HostMmioCW(*this);
  }

  ~HostMmioCW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostAddrSizeOp : public HostIdOp {

  uint64_t addr_;
  size_t size_;

 public:
  void Display(std::ostream &out) override;

  uint64_t GetAddr() const;

  size_t GetSize() const;

  Event *clone() override {
    return new HostAddrSizeOp(*this);
  }

 protected:
  explicit HostAddrSizeOp(uint64_t ts, const size_t parser_identifier,
                          const std::string parser_name, EventType type,
                          std::string name, uint64_t id, uint64_t addr,
                          size_t size)
      : HostIdOp(ts, parser_identifier, parser_name, type,
                 std::move(name), id),
        addr_(addr),
        size_(size) {
  }

  HostAddrSizeOp(const HostAddrSizeOp &other) = default;

  ~HostAddrSizeOp() override = default;

  virtual bool Equal(const Event &other) override;
};

class HostMmioOp : public HostAddrSizeOp {

  int bar_;
  uint64_t offset_;

 public:
  void Display(std::ostream &out) override;

  int GetBar() const;

  uint64_t GetOffset() const;

  Event *clone() override {
    return new HostMmioOp(*this);
  }

 protected:
  explicit HostMmioOp(uint64_t ts, const size_t parser_identifier,
                      const std::string parser_name, EventType type,
                      std::string name, uint64_t id, uint64_t addr,
                      size_t size, int bar, uint64_t offset)
      : HostAddrSizeOp(ts, parser_identifier, parser_name, type,
                       std::move(name), id, addr, size), bar_(bar), offset_(offset) {
  }

  HostMmioOp(const HostMmioOp &other) = default;

  virtual bool Equal(const Event &other) override;
};

class HostMmioR : public HostMmioOp {
 public:
  explicit HostMmioR(uint64_t ts, const size_t parser_identifier,
                     const std::string parser_name, uint64_t id, uint64_t addr,
                     size_t size, int bar, uint64_t offset)
      : HostMmioOp(ts, parser_identifier, parser_name,
                   EventType::kHostMmioRT, "HostMmioR", id, addr, size, bar,
                   offset) {
  }

  HostMmioR(const HostMmioR &other) = default;

  Event *clone() override {
    return new HostMmioR(*this);
  }

  ~HostMmioR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostMmioW : public HostMmioOp {
 public:
  explicit HostMmioW(uint64_t ts, const size_t parser_identifier,
                     const std::string parser_name, uint64_t id, uint64_t addr,
                     size_t size, int bar, uint64_t offset)
      : HostMmioOp(ts, parser_identifier, parser_name,
                   EventType::kHostMmioWT, "HostMmioW", id, addr, size, bar,
                   offset) {
  }

  HostMmioW(const HostMmioW &other) = default;

  Event *clone() override {
    return new HostMmioW(*this);
  }

  ~HostMmioW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostDmaC : public HostIdOp {
 public:
  explicit HostDmaC(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id)
      : HostIdOp(ts, parser_identifier, parser_name,
                 EventType::kHostDmaCT, "HostDmaC", id) {
  }

  HostDmaC(const HostDmaC &other) = default;

  Event *clone() override {
    return new HostDmaC(*this);
  }

  ~HostDmaC() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostDmaR : public HostAddrSizeOp {
 public:
  explicit HostDmaR(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id, uint64_t addr,
                    size_t size)
      : HostAddrSizeOp(ts, parser_identifier, parser_name,
                       EventType::kHostDmaRT, "HostDmaR", id, addr, size) {
  }

  HostDmaR(const HostDmaR &other) = default;

  Event *clone() override {
    return new HostDmaR(*this);
  }

  ~HostDmaR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostDmaW : public HostAddrSizeOp {
 public:
  explicit HostDmaW(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t id, uint64_t addr,
                    size_t size)
      : HostAddrSizeOp(ts, parser_identifier, parser_name,
                       EventType::kHostDmaWT, "HostDmaW", id, addr, size) {
  }

  HostDmaW(const HostDmaW &other) = default;

  Event *clone() override {
    return new HostDmaW(*this);
  }

  ~HostDmaW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostMsiX : public Event {

  uint64_t vec_;

 public:
  explicit HostMsiX(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t vec)
      : Event(ts, parser_identifier, parser_name,
              EventType::kHostMsiXT, "HostMsiX"),
        vec_(vec) {
  }

  HostMsiX(const HostMsiX &other) = default;

  Event *clone() override {
    return new HostMsiX(*this);
  }

  ~HostMsiX() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetVec() const;
};

class HostConf : public Event {

  uint64_t dev_;
  uint64_t func_;
  uint64_t reg_;
  size_t bytes_;
  uint64_t data_;
  bool is_read_;

 public:
  explicit HostConf(uint64_t ts, const size_t parser_identifier,
                    const std::string parser_name, uint64_t dev, uint64_t func,
                    uint64_t reg, size_t bytes, uint64_t data, bool is_read)
      : Event(ts, parser_identifier, parser_name,
              EventType::kHostConfT,
              is_read ? "HostConfRead" : "HostConfWrite"),
        dev_(dev),
        func_(func),
        reg_(reg),
        bytes_(bytes),
        data_(data),
        is_read_(is_read) {
  }

  HostConf(const HostConf &other) = default;

  Event *clone() override {
    return new HostConf(*this);
  }

  ~HostConf() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetDev() const;

  uint64_t GetFunc() const;

  uint64_t GetReg() const;

  size_t GetBytes() const;

  uint64_t GetData() const;

  bool IsRead() const;
};

class HostClearInt : public Event {
 public:
  explicit HostClearInt(uint64_t ts, const size_t parser_identifier,
                        const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::kHostClearIntT, "HostClearInt") {
  }

  HostClearInt(const HostClearInt &other) = default;

  Event *clone() override {
    return new HostClearInt(*this);
  }

  ~HostClearInt() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostPostInt : public Event {
 public:
  explicit HostPostInt(uint64_t ts, const size_t parser_identifier,
                       const std::string parser_name)
      : Event(ts, parser_identifier, parser_name,
              EventType::kHostPostIntT, "HostPostInt") {
  }

  HostPostInt(const HostPostInt &other) = default;

  Event *clone() override {
    return new HostPostInt(*this);
  }

  ~HostPostInt() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class HostPciRW : public Event {

  uint64_t offset_;
  size_t size_;
  bool is_read_;

 public:

  explicit HostPciRW(uint64_t ts, const size_t parser_identifier,
                     const std::string parser_name, uint64_t offset,
                     size_t size, bool is_read)
      : Event(ts, parser_identifier, parser_name,
              EventType::kHostPciRWT, is_read ? "HostPciR" : "HostPciW"),
        offset_(offset),
        size_(size),
        is_read_(is_read) {
  }

  HostPciRW(const HostPciRW &other) = default;

  Event *clone() override {
    return new HostPciRW(*this);
  }

  ~HostPciRW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetOffset() const;

  size_t GetSize() const;

  bool IsRead() const;
};

/* NIC related events */
class NicMsix : public Event {

  uint16_t vec_;
  bool isX_;

 public:

  NicMsix(uint64_t ts, const size_t parser_identifier,
          const std::string parser_name, uint16_t vec, bool isX)
      : Event(ts, parser_identifier, parser_name,
              EventType::kNicMsixT, isX ? "NicMsix" : "NicMsi"),
        vec_(vec),
        isX_(isX) {
  }

  NicMsix(const NicMsix &other) = default;

  Event *clone() override {
    return new NicMsix(*this);
  }

  ~NicMsix() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint16_t GetVec() const;

  bool IsX() const;
};

class NicDma : public Event {

  uint64_t id_;
  uint64_t addr_;
  size_t len_;

 public:

  void Display(std::ostream &out) override;

  uint64_t GetId() const;

  uint64_t GetAddr() const;

  size_t GetLen() const;

  Event *clone() override {
    return new NicDma(*this);
  }

 protected:
  NicDma(uint64_t ts, const size_t parser_identifier,
         const std::string parser_name, EventType type, std::string name,
         uint64_t id, uint64_t addr, size_t len)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        id_(id),
        addr_(addr),
        len_(len) {
  }

  NicDma(const NicDma &other) = default;

  ~NicDma() override = default;

  virtual bool Equal(const Event &other) override;
};

class SetIX : public Event {

  uint64_t intr_;

 public:

  SetIX(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, uint64_t intr)
      : Event(ts, parser_identifier, parser_name, EventType::kSetIXT,
              "SetIX"),
        intr_(intr) {
  }

  SetIX(const SetIX &other) = default;

  Event *clone() override {
    return new SetIX(*this);
  }

  ~SetIX() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  uint64_t GetIntr() const;
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t ts, const size_t parser_identifier,
          const std::string parser_name, uint64_t id, uint64_t addr,
          size_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::kNicDmaIT, "NicDmaI", id, addr, len) {
  }

  NicDmaI(const NicDmaI &other) = default;

  Event *clone() override {
    return new NicDmaI(*this);
  }

  ~NicDmaI() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaEx : public NicDma {
 public:
  NicDmaEx(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::kNicDmaExT, "NicDmaEx", id, addr, len) {
  }

  NicDmaEx(const NicDmaEx &other) = default;

  Event *clone() override {
    return new NicDmaEx(*this);
  }

  ~NicDmaEx() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaEn : public NicDma {
 public:
  NicDmaEn(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::kNicDmaEnT, "NicDmaEn", id, addr, len) {
  }

  NicDmaEn(const NicDmaEn &other) = default;

  Event *clone() override {
    return new NicDmaEn(*this);
  }

  ~NicDmaEn() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::kNicDmaCRT, "NicDmaCR", id, addr, len) {
  }

  NicDmaCR(const NicDmaCR &other) = default;

  Event *clone() override {
    return new NicDmaCR(*this);
  }

  ~NicDmaCR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t id, uint64_t addr,
           size_t len)
      : NicDma(ts, parser_identifier, parser_name,
               EventType::kNicDmaCWT, "NicDmaCW", id, addr, len) {
  }

  NicDmaCW(const NicDmaCW &other) = default;

  Event *clone() override {
    return new NicDmaCW(*this);
  }

  ~NicDmaCW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicMmio : public Event {

  uint64_t off_;
  size_t len_;
  uint64_t val_;

 public:

  virtual void Display(std::ostream &out) override;

  uint64_t GetOff() const;

  size_t GetLen() const;

  uint64_t GetVal() const;

  Event *clone() override {
    return new NicMmio(*this);
  }

 protected:
  NicMmio(uint64_t ts, const size_t parser_identifier,
          const std::string parser_name, EventType type, std::string name,
          uint64_t off, size_t len, uint64_t val)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        off_(off),
        len_(len),
        val_(val) {
  }

  NicMmio(const NicMmio &other) = default;

  ~NicMmio() override = default;

  virtual bool Equal(const Event &other) override;
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t off, size_t len,
           uint64_t val)
      : NicMmio(ts, parser_identifier, parser_name,
                EventType::kNicMmioRT, "NicMmioR", off, len, val) {
  }

  NicMmioR(const NicMmioR &other) = default;

  Event *clone() override {
    return new NicMmioR(*this);
  }

  ~NicMmioR() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicMmioW : public NicMmio {
  bool posted_ = false;

 public:
  NicMmioW(uint64_t ts, const size_t parser_identifier,
           const std::string parser_name, uint64_t off, size_t len,
           uint64_t val, bool posted)
      : NicMmio(ts, parser_identifier, parser_name,
                EventType::kNicMmioWT, "NicMmioW", off, len, val), posted_(posted) {
  }

  NicMmioW(const NicMmioW &other) = default;

  Event *clone() override {
    return new NicMmioW(*this);
  }

  ~NicMmioW() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicTrx : public Event {
  size_t len_;
  bool is_read_;

 public:

  void Display(std::ostream &out) override;

  size_t GetLen() const;

  bool IsRead() const {
    return is_read_;
  }

  Event *clone() override {
    return new NicTrx(*this);
  }

 protected:
  NicTrx(uint64_t ts, const size_t parser_identifier,
         const std::string parser_name, EventType type, std::string name,
         size_t len, bool is_read)
      : Event(ts, parser_identifier, parser_name, type,
              std::move(name)),
        len_(len), is_read_(is_read) {
  }

  NicTrx(const NicTrx &other) = default;

  ~NicTrx() override = default;

  virtual bool Equal(const Event &other) override;
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, size_t len)
      : NicTrx(ts, parser_identifier, parser_name,
               EventType::kNicTxT, "NicTx", len, false) {
  }

  NicTx(const NicTx &other) = default;

  Event *clone() override {
    return new NicTx(*this);
  }

  ~NicTx() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;
};

class NicRx : public NicTrx {
  int port_;

 public:
  NicRx(uint64_t ts, const size_t parser_identifier,
        const std::string parser_name, int port, size_t len)
      : NicTrx(ts, parser_identifier, parser_name,
               EventType::kNicRxT, "NicRx", len, true),
        port_(port) {
  }

  NicRx(const NicRx &other) = default;

  Event *clone() override {
    return new NicRx(*this);
  }

  ~NicRx() override = default;

  void Display(std::ostream &out) override;

  bool Equal(const Event &other) override;

  int GetPort() const;
};

inline std::ostream &operator<<(std::ostream &out, Event &event) {
  event.Display(out);
  return out;
}

inline std::shared_ptr<Event> CloneShared(const std::shared_ptr<Event> &other) {
  if (not other) {
    return {};
  }
  auto raw_ptr = other->clone();
  throw_if_empty(raw_ptr, "CloneShared: cloned raw pointer is null");
  return std::shared_ptr<Event>(raw_ptr);
}

bool IsType(std::shared_ptr<Event> &event_ptr, EventType type);

bool IsType(const Event &event, EventType type);

class EventPrinter : public consumer<std::shared_ptr<Event>>,
                     public cpipe<std::shared_ptr<Event>> {
  std::ostream &out_;

  inline void print(const std::shared_ptr<Event> &event) {
    throw_if_empty(event, event_is_null);
    out_ << *event << std::endl;
    out_.flush();
  }

 public:

  explicit EventPrinter(std::ostream &out) : out_(out) {
  }

  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> src_chan
  ) override {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(src_chan, channel_is_null);

    std::optional<std::shared_ptr<Event>> msg;
    for (msg = co_await src_chan->Pop(resume_executor); msg.has_value();
         msg = co_await src_chan->Pop(resume_executor)) {
      const std::shared_ptr<Event> &event = msg.value();
      print(event);
    }

    std::cout << "event printer exited" << std::endl;
    co_return;
  }

  concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> src_chan,
      std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> tar_chan
  ) override {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(src_chan, channel_is_null);
    throw_if_empty(tar_chan, channel_is_null);

    std::optional<std::shared_ptr<Event>> msg;
    for (msg = co_await src_chan->Pop(resume_executor); msg.has_value();
         msg = co_await src_chan->Pop(resume_executor)) {
      std::shared_ptr<Event> event = msg.value();
      print(event);
      const bool was_pushed = co_await tar_chan->Push(resume_executor, event);
      throw_on(not was_pushed,
               "EventPrinter::process: Could not push to target channel");
    }

    co_await tar_chan->CloseChannel(resume_executor);
    std::cout << "event printer exited" << std::endl;
    co_return;
  }
};

struct EventComperator {
  bool operator()(const std::shared_ptr<Event> &ev1,
                  const std::shared_ptr<Event> &ev2) const {
    return ev1->GetTs() > ev2->GetTs();
  }
};

inline std::string GetTypeStr(std::shared_ptr<Event> event) {
  if (not event) {
    return "";
  }
  std::stringstream sss;
  sss << event->GetType();
  return std::move(sss.str());
}

#endif  // SIMBRICKS_TRACE_EVENTS_H_
