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

class E_NIC_MSIX : public Event {
 public:
  uint16_t vec_;

  E_NIC_MSIX(uint64_t ts, uint16_t vec) : Event(ts), vec_(vec) {
  }

  void display(std::ostream &os) override {
    os << "N.MSIX ";
    Event::display(os);
    os << ", vec=" << vec_;
  }
};

class E_NIC_DMA : public Event {
 public:
  uint64_t id_;
  uint64_t addr_;
  uint64_t len_;

  E_NIC_DMA(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : Event(ts), id_(id), addr_(addr), len_(len) {
  }

  void display(std::ostream &os) override {
    Event::display(os);
    os << "id=" << std::hex << id_ << ", addr=" << std::hex << addr_ << ", size=" << len_;
  }
};

class E_NIC_DMA_I : public E_NIC_DMA {
 public:
  E_NIC_DMA_I(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : E_NIC_DMA(ts, id, addr, len) {
  }

  void display(std::ostream &os) override {
    os << "N.DMAI ";
    E_NIC_DMA::display(os);
  }
};

class E_NIC_DMA_E : public E_NIC_DMA {
 public:
  E_NIC_DMA_E(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : E_NIC_DMA(ts, id, addr, len) {
  }

  void display(std::ostream &os) {
    os << "N.DMAC ";
    E_NIC_DMA::display(os);
  }
};

class E_NIC_DMA_CR : public E_NIC_DMA {
 public:
  E_NIC_DMA_CR(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : E_NIC_DMA(ts, id, addr, len) {
  }

  void display(std::ostream &os) {
    os << "N.DMACR ";
    E_NIC_DMA::display(os);
  }
};

class E_NIC_DMA_CW : public E_NIC_DMA {
 public:
  E_NIC_DMA_CW(uint64_t ts, uint64_t id, uint64_t addr, uint64_t len)
      : E_NIC_DMA(ts, id, addr, len) {
  }

  void display(std::ostream &os) {
    os << "N.DMACW ";
    E_NIC_DMA::display(os);
  }
};

class E_NIC_MMIO : public Event {
 public:
  uint64_t off_;
  uint64_t len_;
  uint64_t val_;

  E_NIC_MMIO(uint64_t ts, uint64_t off, uint64_t len, uint64_t val)
      : Event(ts), off_(off), len_(len), val_(val) {
  }

  virtual void display(std::ostream &os) override {
    Event::display(os);
    os << "off=" << std::hex << off_ << ", len=" << len_ << " val=" << std::hex << val_;
  }
};

class E_NIC_MMIO_R : public E_NIC_MMIO {
 public:
  E_NIC_MMIO_R(uint64_t ts, uint64_t off, uint64_t len, uint64_t val)
      : E_NIC_MMIO(ts, off, len, val) {
  }

  void display(std::ostream &os) override {
    os << "N.MMIOR ";
    E_NIC_MMIO::display(os);
  }
};

class E_NIC_MMIO_W : public E_NIC_MMIO {
 public:
  E_NIC_MMIO_W(uint64_t ts, uint64_t off, uint64_t len, uint64_t val)
      : E_NIC_MMIO(ts, off, len, val) {
  }

  void display(std::ostream &os) override {
    os << "N.MMIOW ";
    E_NIC_MMIO::display(os);
  }
};

class E_NIC_TRX : public Event {
 public:
  uint16_t len_;

  E_NIC_TRX(uint64_t ts, uint16_t len) : Event(ts), len_(len) {
  }

  void display(std::ostream &os) {
    Event::display(os);
    os << ", len=" << len_;
  }
};

class E_NIC_TX : public E_NIC_TRX {
 public:
  E_NIC_TX(uint64_t ts, uint16_t len) : E_NIC_TRX(ts, len) {
  }

  void display(std::ostream &os) {
    os << "N.TX ";
    E_NIC_TRX::display(os);
  }
};

class E_NIC_RX : public E_NIC_TRX {
 public:
  E_NIC_RX(uint64_t ts, uint16_t len) : E_NIC_TRX(ts, len) {
  }

  void display(std::ostream &os) {
    os << "N.RX ";
    E_NIC_TRX::display(os);
  }
};

inline std::ostream &operator<<(std::ostream &os, Event &e) {
  e.display(os);
  return os;
}

struct EventComperator {
  bool operator() (const Event &e1, const Event &e2) {
    return e1.timestamp_ < e2.timestamp_;
  }
};


#endif  // SIMBRICKS_TRACE_EVENTS_H_
