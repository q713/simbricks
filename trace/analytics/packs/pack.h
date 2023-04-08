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

#ifndef SIMBRICKS_TRACE_EVENT_PACK_H_
#define SIMBRICKS_TRACE_EVENT_PACK_H_

#include <iostream>
#include <memory>
#include <vector>

#include "trace/events/events.h"

using event_t = std::shared_ptr<Event>;

void write_ident(std::ostream &out, unsigned ident) {
  if (ident == 0)
    return;

  for (size_t i = 0; i < ident; i++) {
    out << "\t";
  }
}

inline uint64_t get_pack_id() {
  static uint64_t next_id = 0;
  return next_id++;
}

// TODO: maybe add a trigger type
enum pack_type {
  CALL_PACK,
  DMA_PACK,
  MMIO_PACK,
  SE_PACK,
  ETH_PACK,
  MSIX_PACK,
  HOST_INT_PACK
};

inline std::ostream &operator<<(std::ostream &os, pack_type t) {
  switch (t) {
    case pack_type::CALL_PACK:
      os << "call_pack";
      break;
    case pack_type::DMA_PACK:
      os << "dma_pack";
      break;
    case pack_type::MMIO_PACK:
      os << "mmio_pack";
      break;
    case pack_type::ETH_PACK:
      os << "eth_pack";
      break;
    case pack_type::SE_PACK:
      os << "single_event_pack";
      break;
    case pack_type::MSIX_PACK:
      os << "msix_pack";
      break;
    case pack_type::HOST_INT_PACK:
      os << "host_int_pack";
      break;
    default:
      os << "could not represent given pack type";
      break;
  }
  return os;
}

struct event_pack {
  using pack_t = std::shared_ptr<event_pack>;

  uint64_t id_;
  pack_type type_;
  // Stacks stack_; // TODO: not whole stacks is within a stack, but each event
  // on its own
  std::vector<event_t> events_;

  pack_t triggered_by_ = nullptr;
  std::vector<pack_t> triggered_;

  bool is_pending_ = true;

  virtual void display(std::ostream &out, unsigned ident) {
    write_ident(out, ident);
    out << "id: " << (unsigned long long)id_ << ", kind: " << type_
        << std::endl;
    write_ident(out, ident);
    out << "was triggered? " << (triggered_by_ != nullptr) << std::endl;
    write_ident(out, ident);
    out << "triggered packs? ";
    for (auto &p : triggered_) {
      out << (unsigned long long)p->id_ << ", ";
    }
    out << std::endl;
    for (event_t event : events_) {
      write_ident(out, ident);
      out << *event << std::endl;
    }
  }

  inline virtual void display(std::ostream &out) {
    display(out, 0);
  }

  inline pack_type get_type() {
    return type_;
  }

  virtual bool is_pending() {
    return is_pending_;
  }

  virtual bool is_complete() {
    return not is_pending();
  }

  virtual bool add_if_triggered(pack_t pack_ptr) {
    return false;
  }

  virtual bool add_on_match(event_t event_ptr) {
    return false;
  }

  bool set_triggered_by(pack_t trigger) {
    if (not triggered_by_) {
      triggered_by_ = trigger;
      return true;
    }
    return false;
  }

  virtual ~event_pack() = default;

 protected:
  event_pack(pack_type t) : id_(get_pack_id()), type_(t) {
  }

  void add_to_pack(event_t event_ptr) {
    if (event_ptr) {
      events_.push_back(event_ptr);
    }
  }

  void add_triggered(pack_t pack_ptr) {
    if (pack_ptr) {
      triggered_.push_back(pack_ptr);
    }
  }

  bool potentially_triggered(pack_t pack_ptr) {
    if (not pack_ptr /*or pack_ptr->is_pending()*/ or pack_ptr.get() == this) {
      return false;
    }

    return true;
  }

  bool ends_with_offset(uint64_t addr, uint64_t off) {
    size_t lz = std::__countl_zero(off);
    uint64_t mask = lz == 64 ? 0xffffffffffffffff : (1 << (64 - lz)) - 1;
    uint64_t check = addr & mask;
    return check == off;
  }
};

bool is_type(std::shared_ptr<event_pack> pack, pack_type type) {
  if (not pack) {
    return false;
  }
  return pack->type_ == type;
}

#endif  // SIMBRICKS_TRACE_EVENT_PACK_H_
