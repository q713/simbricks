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

#include "corobelt.h"
#include "traceEnvironment.h"
#include "events.h"

using event_t = std::shared_ptr<Event>;
struct event_pack;
using pack_t = std::shared_ptr<event_pack>;

inline void write_ident(std::ostream &out, unsigned ident) {
  if (ident == 0)
    return;

  for (size_t i = 0; i < ident; i++) {
    out << "\t";
  }
}

enum pack_type {
  host_call,
  host_msix,
  host_mmio,
  host_dma,
  host_int,
  nic_dma,
  nic_mmio,
  nic_eth,
  nic_msix,
  generic_single
};

inline std::ostream &operator<<(std::ostream &os, pack_type t) {
  switch (t) {
    case pack_type::host_call:
      os << "host_call";
      break;
    case pack_type::host_msix:
      os << "host_msix";
      break;
    case pack_type::host_mmio:
      os << "host_mmio";
      break;
    case pack_type::host_dma:
      os << "host_dma";
      break;
    case pack_type::host_int:
      os << "host_int";
      break;
    case pack_type::nic_dma:
      os << "nic_dma";
      break;
    case pack_type::nic_mmio:
      os << "nic_mmio";
      break;
    case pack_type::nic_eth:
      os << "nic_eth";
      break;
    case pack_type::nic_msix:
      os << "nic_msix";
      break;
    case pack_type::generic_single:
      os << "generic_single";
      break;
    default:
      os << "could not represent given pack type";
      break;
  }
  return os;
}

struct event_pack {
  sim::trace::env::trace_environment &env_;

  uint64_t id_;
  pack_type type_;
  // Stacks stack_; // TODO: not whole stacks is within a stack, but each event
  // on its own
  std::vector<event_t> events_;

  pack_t triggered_by_ = nullptr;
  std::vector<pack_t> triggered_;

  bool is_pending_ = true;
  bool is_relevant_ = false;

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

  void mark_as_done() {
    is_pending_ = false;
  }

  bool is_pending() {
    return is_pending_;
  }

  bool is_complete() {
    return not is_pending();
  }

  inline void mark_as_relevant() {
    is_relevant_ = true;
  }

  inline void mark_as_non_relevant() {
    is_relevant_ = false;
  }

  virtual uint64_t get_smallest_cimpletion_ts() {
    if (is_pending()) {
      return 0xFFFFFFFFFFFFFFFF;
    }
    return events_.back()->timestamp_;
  }

  bool set_triggered_by(pack_t trigger) {
    if (not triggered_by_) {
      triggered_by_ = trigger;
      return true;
    }
    return false;
  }

  virtual ~event_pack() = default;

  event_pack(sim::trace::env::trace_environment &env, pack_type t)
      : env_(env), id_(env.get_next_pack_id()), type_(t) {
  }

  bool is_potential_add(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    if (is_complete()) {
      return false;
    }

    if (not events_.empty()) {
      auto first = events_.front();
      if (first->getIdent() != event_ptr->getIdent()) {
        return false;
      }
    }

    return true;
  }

  virtual bool add_to_pack(event_t event_ptr) = 0;

  virtual bool add_triggered(pack_t pack_ptr) {
    if (pack_ptr) {
      triggered_.push_back(pack_ptr);
      return true;
    }
    return false;
  }
};

struct host_call_pack : public event_pack {
  event_t call_pack_entry_ = nullptr;
  event_t syscall_return_ = nullptr;
  bool transmits_ = false;
  bool receives_ = false;

  void set_call_pack_entry(event_t event_ptr) {
    call_pack_entry_ = event_ptr;
  }

  void set_syscall_return(event_t event_ptr) {
    syscall_return_ = event_ptr;
  }

  // for a call pack we want to know which event exactly caused another span
  std::unordered_map<event_t, pack_t> triggered_;
  std::list<event_t> send_trigger_;
  std::list<event_t> receiver_;

  host_call_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::host_call) {
  }

  ~host_call_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      std::cout << "is not a potential add" << std::endl;
      return false;
    }

    if (not is_type(event_ptr, EventType::HostCall_t)) {
      return false;
    }

    if (env_.is_sys_entry(event_ptr)) {
      if (call_pack_entry_) {
        is_pending_ = false;
        syscall_return_ = events_.back();
        // std::cout << "found call stack end" << std::endl;
        // this->display(std::cout);
        // std::cout << std::endl;
        return false;
      }
      is_pending_ = true;
      call_pack_entry_ = event_ptr;
      events_.push_back(event_ptr);
      return true;
    }

    if (not call_pack_entry_) {
      return false;
    }

    if (env_.is_driver_tx(event_ptr)) {
      transmits_ = true;
      send_trigger_.push_back(event_ptr);

      // TODO: where does the kernel actually "receive" the packet
    } else if (env_.is_driver_rx(event_ptr)) {
      receives_ = true;
      receiver_.push_back(event_ptr);
    }

    // is_relevant_ = is_relevant_ || transmits_ || receives_;
    // sim::analytics::conf::LINUX_NET_STACK_FUNC_INDICATOR.contains(
    //      host_call->func_);

    events_.push_back(event_ptr);
    return true;
  }
};

struct host_int_pack : public event_pack {
  event_t host_post_int_ = nullptr;
  event_t host_clear_int_ = nullptr;

  host_int_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::host_int) {
  }

  ~host_int_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    if (is_type(event_ptr, EventType::HostPostInt_t)) {
      if (host_post_int_) {
        return false;
      }
      host_post_int_ = event_ptr;

    } else if (is_type(event_ptr, EventType::HostClearInt_t)) {
      if (not host_post_int_ or host_clear_int_) {
        return false;
      }
      host_clear_int_ = event_ptr;
      is_pending_ = false;

    } else {
      return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct host_dma_pack : public event_pack {
  // HostDmaW_t or HostDmaR_t
  event_t host_dma_execution_ = nullptr;
  bool is_read_ = true;
  // HostDmaC_t
  event_t host_dma_completion_ = nullptr;

  host_dma_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::host_dma) {
  }

  ~host_dma_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    switch (event_ptr->getType()) {
      case EventType::HostDmaW_t:
      case EventType::HostDmaR_t: {
        if (host_dma_execution_) {
          return false;
        }

        is_read_ = is_type(event_ptr, EventType::HostDmaR_t);
        host_dma_execution_ = event_ptr;
        break;
      }

      case EventType::HostDmaC_t: {
        if (not host_dma_execution_) {
          return false;
        }

        auto exec =
            std::static_pointer_cast<HostAddrSizeOp>(host_dma_execution_);
        auto comp = std::static_pointer_cast<HostDmaC>(event_ptr);
        if (exec->id_ != comp->id_) {
          return false;
        }

        host_dma_completion_ = event_ptr;
        is_pending_ = false;
        break;
      }

      default:
        return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct host_mmio_pack : public event_pack {
  // issue, either host_mmio_w_ or host_mmio_r_
  event_t host_mmio_issue_ = nullptr;
  bool is_read_ = false;
  event_t host_msi_read_resp_ = nullptr;
  bool pci_msix_desc_addr_before_;
  event_t im_mmio_resp_ = nullptr;
  // completion, either host_mmio_cw_ or host_mmio_cr_
  event_t completion_ = nullptr;

  explicit host_mmio_pack(sim::trace::env::trace_environment &env,
                          bool pci_msix_desc_addr_before)
      : event_pack(env, pack_type::host_mmio),
        pci_msix_desc_addr_before_(pci_msix_desc_addr_before) {
  }

  ~host_mmio_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    switch (event_ptr->getType()) {
      case EventType::HostMmioW_t: {
        if (host_mmio_issue_) {
          return false;
        }
        is_read_ = false;
        host_mmio_issue_ = event_ptr;
        break;
      }
      case EventType::HostMmioR_t: {
        if (host_mmio_issue_ and not pci_msix_desc_addr_before_) {
          return false;
        }

        if (pci_msix_desc_addr_before_) {
          if (is_read_ or not host_mmio_issue_ or not im_mmio_resp_) {
            return false;
          }

          auto issue =
              std::static_pointer_cast<HostAddrSizeOp>(host_mmio_issue_);
          auto th = std::static_pointer_cast<HostMmioR>(event_ptr);
          if (issue->id_ != th->id_) {
            return false;
          }

          host_msi_read_resp_ = event_ptr;
          is_pending_ = false;
        } else {
          is_read_ = true;
          host_mmio_issue_ = event_ptr;
        }
        break;
      }

      case EventType::HostMmioImRespPoW_t: {
        if (not host_mmio_issue_ or is_read_ or im_mmio_resp_) {
          return false;
        }
        if (host_mmio_issue_->timestamp_ != event_ptr->timestamp_) {
          return false;
        }
        im_mmio_resp_ = event_ptr;
        break;
      }

      case EventType::HostMmioCW_t:
      case EventType::HostMmioCR_t: {
        if (is_type(event_ptr, EventType::HostMmioCW_t)) {
          if (is_read_ or not host_mmio_issue_ or not im_mmio_resp_) {
            return false;
          }
        } else {
          if (not is_read_ or not host_mmio_issue_) {
            return false;
          }
        }

        if (pci_msix_desc_addr_before_) {
          return false;
        }

        auto issue = std::static_pointer_cast<HostAddrSizeOp>(host_mmio_issue_);
        auto comp = std::static_pointer_cast<HostIdOp>(event_ptr);
        if (issue->id_ != comp->id_) {
          return false;
        }
        completion_ = event_ptr;
        is_pending_ = false;
        break;
      }

      default:
        return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct host_msix_pack : public event_pack {
  event_t host_msix_ = nullptr;

  host_msix_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::host_msix) {
  }

  ~host_msix_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr) or
        not is_type(event_ptr, EventType::HostMsiX_t)) {
      return false;
    }

    host_msix_ = event_ptr;
    events_.push_back(event_ptr);
    return true;
  }
};

struct nic_msix_pack : public event_pack {
  event_t nic_msix_ = nullptr;

  nic_msix_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::nic_msix) {
  }

  ~nic_msix_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    if (is_type(event_ptr, EventType::NicMsix_t)) {
      if (nic_msix_) {
        return false;
      }
      nic_msix_ = event_ptr;

    } else {
      return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct nic_mmio_pack : public event_pack {
  // nic action nic_mmio_w_ or nic_mmio_r_
  event_t action_ = nullptr;
  bool is_read_ = false;

  nic_mmio_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::nic_mmio) {
  }

  ~nic_mmio_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    if (is_type(event_ptr, EventType::NicMmioR_t)) {
      is_read_ = true;
    } else if (is_type(event_ptr, EventType::NicMmioW_t)) {
      is_read_ = false;
    } else {
      return false;
    }

    is_pending_ = false;
    action_ = event_ptr;
    events_.push_back(event_ptr);
    return true;
  }
};

struct nic_dma_pack : public event_pack {
  // NicDmaI_t
  event_t dma_issue_ = nullptr;
  // NicDmaEx_t
  event_t nic_dma_execution_ = nullptr;
  // NicDmaCW_t or NicDmaCR_t
  event_t nic_dma_completion_ = nullptr;
  bool is_read_ = true;

  nic_dma_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::nic_dma) {
  }

  ~nic_dma_pack() = default;

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    switch (event_ptr->getType()) {
      case EventType::NicDmaI_t: {
        if (dma_issue_) {
          return false;
        }
        dma_issue_ = event_ptr;
        break;
      }

      case EventType::NicDmaEx_t: {
        if (not dma_issue_) {
          return false;
        }
        auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
        auto exec = std::static_pointer_cast<NicDmaEx>(event_ptr);
        if (issue->id_ != exec->id_ or issue->addr_ != exec->addr_) {
          return false;
        }
        nic_dma_execution_ = event_ptr;
        break;
      }

      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t: {
        if (not dma_issue_ or not nic_dma_execution_) {
          return false;
        }

        is_read_ = is_type(event_ptr, EventType::NicDmaCR_t);

        auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
        auto comp = std::static_pointer_cast<NicDma>(event_ptr);
        if (issue->id_ != comp->id_ or issue->addr_ != comp->addr_) {
          return false;
        }

        nic_dma_completion_ = event_ptr;
        is_pending_ = false;
        break;
      }

      default:
        return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct nic_eth_pack : public event_pack {
  // NicTx or NicRx
  event_t tx_rx_ = nullptr;
  bool is_send_ = false;

  nic_eth_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::nic_eth) {
  }

  ~nic_eth_pack() = default;

  inline bool is_transmit() {
    return is_send_;
  }

  inline bool is_receive() {
    return not is_transmit();
  }

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    if (is_type(event_ptr, EventType::NicTx_t)) {
      is_send_ = true;
    } else if (is_type(event_ptr, EventType::NicRx_t)) {
      is_send_ = false;
    } else {
      return false;
    }

    is_pending_ = false;
    tx_rx_ = event_ptr;
    events_.push_back(event_ptr);
    return true;
  }
};

struct generic_single_pack : public event_pack {
  event_t event_p_ = nullptr;

  generic_single_pack(sim::trace::env::trace_environment &env)
      : event_pack(env, pack_type::generic_single) {
  }

  bool add_to_pack(event_t event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    if (event_p_) {
      return false;
    }

    event_p_ = event_ptr;
    is_pending_ = false;
    events_.push_back(event_ptr);
    return true;
  }
};

inline bool is_type(std::shared_ptr<event_pack> pack, pack_type type) {
  if (not pack) {
    return false;
  }
  return pack->type_ == type;
}

struct pack_printer : public sim::corobelt::consumer<pack_t> {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<pack_t> *producer_task) {
    if (not producer_task) {
      co_return;
    }

    pack_t next_pack = nullptr;
    while (*producer_task) {
      next_pack = producer_task->get();
      next_pack->display(std::cout);
    }

    co_return;
  }

  pack_printer() : sim::corobelt::consumer<pack_t>() {
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_PACK_H_
