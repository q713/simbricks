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
#include <ostream>
#include <string>

class Event {
 public:
  uint64_t timestamp_;

  explicit Event(uint64_t ts) : timestamp_(ts) {
  }

  virtual void display(std::ostream &os) {
    os << "Event: timestamp=" << timestamp_ << " ";
  }
};

/* Host related events */
// TODO: add more events
class HostCall : public Event {
 public:
  const std::string &func_;

  HostCall(uint64_t ts, const std::string &func) : Event(ts), func_(func) {
  }

  void display(std::ostream &os) override {
    os << "H.CALL ";
    Event::display(os);
    os << "func=" << func_;
  }
};

/* NIC related events */
class NicMsix : public Event {
 public:
  uint16_t vec_;

  NicMsix(uint64_t ts, uint16_t vec) : Event(ts), vec_(vec) {
  }

  void display(std::ostream &os) override {
    os << "N.MSIX ";
    Event::display(os);
    os << ", vec=" << vec_;
  }
};

class NicDma : public Event {
 public:
  uint64_t id_;
  uint64_t addr_;
  uint64_t len_;

  NicDma(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : Event(ts), id_(id), addr_(addr), len_(len) {
  }

  void display(std::ostream &os) override {
    Event::display(os);
    os << "id=" << std::hex << id_ << ", addr=" << std::hex << addr_
       << ", size=" << len_;
  }
};

class NicDmaI : public NicDma {
 public:
  NicDmaI(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMAI ";
    NicDma::display(os);
  }
};

class NicDmaE : public NicDma {
 public:
  NicDmaE(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, id, addr, len) {
  }

  void display(std::ostream &os) {
    os << "N.DMAC ";
    NicDma::display(os);
  }
};

class NicDmaCR : public NicDma {
 public:
  NicDmaCR(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, id, addr, len) {
  }

  void display(std::ostream &os) {
    os << "N.DMACR ";
    NicDma::display(os);
  }
};

class NicDmaCW : public NicDma {
 public:
  NicDmaCW(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : NicDma(ts, id, addr, len) {
  }

  void display(std::ostream &os) {
    os << "N.DMACW ";
    NicDma::display(os);
  }
};

class NicMmio : public Event {
 public:
  uint64_t off_;
  uint64_t len_;
  uint64_t val_;

  NicMmio(uint64_t ts, uint64_t off, uint64_t len, uint64_t val)
      : Event(ts), off_(off), len_(len), val_(val) {
  }

  virtual void display(std::ostream &os) override {
    Event::display(os);
    os << "off=" << std::hex << off_ << ", len=" << len_ << " val=" << std::hex
       << val_;
  }
};

class NicMmioR : public NicMmio {
 public:
  NicMmioR(uint64_t ts, uint64_t off, uint64_t len, uint64_t val)
      : NicMmio(ts, off, len, val) {
  }

  void display(std::ostream &os) override {
    os << "N.MMIOR ";
    NicMmio::display(os);
  }
};

class NicMmioW : public NicMmio {
 public:
  NicMmioW(uint64_t ts, uint64_t off, uint64_t len, uint64_t val)
      : NicMmio(ts, off, len, val) {
  }

  void display(std::ostream &os) override {
    os << "N.MMIOW ";
    NicMmio::display(os);
  }
};

class NicTrx : public Event {
 public:
  uint16_t len_;

  NicTrx(uint64_t ts, uint16_t len) : Event(ts), len_(len) {
  }

  void display(std::ostream &os) {
    Event::display(os);
    os << ", len=" << len_;
  }
};

class NicTx : public NicTrx {
 public:
  NicTx(uint64_t ts, uint16_t len) : NicTrx(ts, len) {
  }

  void display(std::ostream &os) {
    os << "N.TX ";
    NicTrx::display(os);
  }
};

class NicRx : public NicTrx {
 public:
  NicRx(uint64_t ts, uint16_t len) : NicTrx(ts, len) {
  }

  void display(std::ostream &os) {
    os << "N.RX ";
    NicTrx::display(os);
  }
};

inline std::ostream &operator<<(std::ostream &os, Event &e) {
  e.display(os);
  return os;
}

struct EventComperator {
  bool operator()(const Event &e1, const Event &e2) {
    return e1.timestamp_ < e2.timestamp_;
  }
};

#endif  // SIMBRICKS_TRACE_EVENTS_H_
