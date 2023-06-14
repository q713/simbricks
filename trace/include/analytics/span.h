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
#include <optional>

#include "util/exception.h"
#include "corobelt/corobelt.h"
#include "events/events.h"
#include "env/traceEnvironment.h"

inline void write_ident(std::ostream &out, unsigned ident) {
  if (ident == 0)
    return;

  for (size_t i = 0; i < ident; i++) {
    out << "\t";
  }
}

enum span_type {
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

inline std::ostream &operator<<(std::ostream &os, span_type t) {
  switch (t) {
    case span_type::host_call:os << "host_call";
      break;
    case span_type::host_msix:os << "host_msix";
      break;
    case span_type::host_mmio:os << "host_mmio";
      break;
    case span_type::host_dma:os << "host_dma";
      break;
    case span_type::host_int:os << "host_int";
      break;
    case span_type::nic_dma:os << "nic_dma";
      break;
    case span_type::nic_mmio:os << "nic_mmio";
      break;
    case span_type::nic_eth:os << "nic_eth";
      break;
    case span_type::nic_msix:os << "nic_msix";
      break;
    case span_type::generic_single:os << "generic_single";
      break;
    default:os << "could not represent given span type";
      break;
  }
  return os;
}

struct EventSpan {
  uint64_t id_;
  uint64_t source_id_;
  span_type type_;
  std::vector<std::shared_ptr<Event>> events_;

  std::shared_ptr<EventSpan> parent_ = nullptr;
  std::vector<std::shared_ptr<EventSpan>> children_;

  bool is_pending_ = true;
  bool is_relevant_ = false;

  uint64_t trace_id_;

  virtual void display(std::ostream &out, unsigned ident) {
    write_ident(out, ident);
    out << "id: " << (unsigned long long) id_;
    out << ", source_id: " << (unsigned long long) source_id_;
    out << ", kind: " << type_ << std::endl;
    write_ident(out, ident);
    out << "has parent? " << (parent_ != nullptr) << std::endl;
    write_ident(out, ident);
    out << "children? ";
    for (auto &p : children_) {
      out << (unsigned long long) p->id_ << ", ";
    }
    out << std::endl;
    for (std::shared_ptr<Event> event : events_) {
      write_ident(out, ident);
      out << *event << std::endl;
    }
  }

  inline virtual void display(std::ostream &out) {
    display(out, 0);
  }

  inline uint64_t get_id() {
    return id_;
  }

  inline span_type get_type() const {
    return type_;
  }

  inline uint64_t get_source_id() const {
    return source_id_;
  }

  inline uint64_t get_trace_id() const {
    return trace_id_;
  }

  inline void set_trace_id(uint64_t trace_id) {
    trace_id_ = trace_id;
  }

  void mark_as_done() {
    is_pending_ = false;
  }

  bool is_pending() const {
    return is_pending_;
  }

  bool is_complete() const {
    return not is_pending();
  }

  inline void mark_as_relevant() {
    is_relevant_ = true;
  }

  inline void mark_as_non_relevant() {
    is_relevant_ = false;
  }

  uint64_t get_starting_ts() {
    if (events_.empty()) {
      return 0xFFFFFFFFFFFFFFFF;
    }

    std::shared_ptr<Event> event_ptr = events_.front();
    if (event_ptr) {
      return event_ptr->timestamp_;
    }
    return 0xFFFFFFFFFFFFFFFF;
  }

  uint64_t get_completion_ts() {
    if (events_.empty() or is_pending()) {
      return 0xFFFFFFFFFFFFFFFF;
    }

    std::shared_ptr<Event> event_ptr = events_.back();
    if (event_ptr) {
      return event_ptr->timestamp_;
    }
    return 0xFFFFFFFFFFFFFFFF;
  }

  bool set_parent(std::shared_ptr<EventSpan> parent_span) {
    if (not parent_ and parent_span and
        parent_span->get_starting_ts() < get_starting_ts()) {
      parent_ = parent_span;
      return true;
    }
    return false;
  }

  bool has_parent() const {
    return parent_ != nullptr;
  }

  std::shared_ptr<EventSpan> get_parent() const {
    return parent_;
  }

  bool add_children(std::shared_ptr<EventSpan> child_span) {
    if (child_span and child_span.get() != this and
        get_starting_ts() < child_span->get_starting_ts()) {
      children_.push_back(child_span);
      return true;
    }

    return false;
  }

  virtual ~EventSpan() = default;

  explicit EventSpan(uint64_t source_id, span_type t)
      : id_(trace_environment::get_next_span_id()),
        source_id_(source_id),
        type_(t) {
  }

  bool is_potential_add(std::shared_ptr<Event> event_ptr) {
    if (not event_ptr) {
      return false;
    }

    if (is_complete()) {
      return false;
    }

    if (not events_.empty()) {
      auto first = events_.front();
      if (first->get_parser_ident() != event_ptr->get_parser_ident()) {
        return false;
      }
    }

    return true;
  }

  virtual bool add_to_span(std::shared_ptr<Event> event_ptr) = 0;
};

inline std::ostream &operator<<(std::ostream &os, EventSpan &span) {
  span.display(os);
  return os;
}

struct HostCallSpan : public EventSpan {
  std::shared_ptr<Event> call_span_entry_ = nullptr;
  std::shared_ptr<Event> syscall_return_ = nullptr;
  bool transmits_ = false;
  bool receives_ = false;

  void set_call_pack_entry(std::shared_ptr<Event> event_ptr) {
    call_span_entry_ = event_ptr;
  }

  void set_syscall_return(std::shared_ptr<Event> event_ptr) {
    syscall_return_ = event_ptr;
  }

  explicit HostCallSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::host_call) {
  }

  ~HostCallSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      std::cout << "is not a potential add" << std::endl;
      return false;
    }

    if (not is_type(event_ptr, EventType::HostCall_t)) {
      return false;
    }

    if (trace_environment::is_sys_entry(event_ptr)) {
      if (call_span_entry_) {
        is_pending_ = false;
        syscall_return_ = events_.back();
        // std::cout << "found call stack end" << std::endl;
        // this->display(std::cout);
        // std::cout << std::endl;
        return false;
      }
      is_pending_ = true;
      call_span_entry_ = event_ptr;
      events_.push_back(event_ptr);
      return true;
    }

    // TODO: clean up this code!!!

    // create a default fallback for these events
    if (not call_span_entry_) {
      //return false;
      is_pending_ = true;
      call_span_entry_ = event_ptr;
      events_.push_back(event_ptr);
      return true;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct HostIntSpan : public EventSpan {
  std::shared_ptr<Event> host_post_int_ = nullptr;
  std::shared_ptr<Event> host_clear_int_ = nullptr;

  explicit HostIntSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::host_int) {
  }

  ~HostIntSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
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

struct HostDmaSpan : public EventSpan {
  // HostDmaW_t or HostDmaR_t
  std::shared_ptr<Event> host_dma_execution_ = nullptr;
  bool is_read_ = true;
  // HostDmaC_t
  std::shared_ptr<Event> host_dma_completion_ = nullptr;

  explicit HostDmaSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::host_dma) {
  }

  ~HostDmaSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    switch (event_ptr->get_type()) {
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
        if (not host_dma_execution_ or host_dma_completion_) {
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

      default:return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct HostMmioSpan : public EventSpan {
  // issue, either host_mmio_w_ or host_mmio_r_
  std::shared_ptr<Event> host_mmio_issue_ = nullptr;
  bool is_read_ = false;
  std::shared_ptr<Event> host_msi_read_resp_ = nullptr;
  bool pci_before_ = false;
  std::shared_ptr<Event> im_mmio_resp_ = nullptr;
  // completion, either host_mmio_cw_ or host_mmio_cr_
  std::shared_ptr<Event> completion_ = nullptr;

  explicit HostMmioSpan(uint64_t source_id, bool pci_before)
      : EventSpan(source_id, span_type::host_mmio),
        pci_before_(pci_before) {
  }

  ~HostMmioSpan() = default;

  inline bool is_after_pci() const {
    return pci_before_;
  }

  inline bool is_read() const {
    return is_read_;
  }

  inline bool is_write() const {
    return not is_read_;
  }

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    switch (event_ptr->get_type()) {
      case EventType::HostMmioW_t: {
        if (host_mmio_issue_) {
          return false;
        }
        is_read_ = false;
        host_mmio_issue_ = event_ptr;
        break;
      }
      case EventType::HostMmioR_t: {
        if (host_mmio_issue_ and not pci_before_) {
          return false;
        }

        if (pci_before_) {
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

        if (pci_before_ or completion_) {
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

      default:return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct HostMsixSpan : public EventSpan {
  std::shared_ptr<Event> host_msix_ = nullptr;
  std::shared_ptr<Event> host_dma_c_ = nullptr;

  explicit HostMsixSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::host_msix) {
  }

  ~HostMsixSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    if (is_type(event_ptr, EventType::HostMsiX_t)) {
      if (host_msix_) {
        return false;
      }
      host_msix_ = event_ptr;
      is_pending_ = true;

    } else if (is_type(event_ptr, EventType::HostDmaC_t)) {
      if (not host_msix_ or host_dma_c_) {
        return false;
      }

      auto dma = std::static_pointer_cast<HostDmaC>(event_ptr);
      if (dma->id_ != 0) {
        return false;
      }
      host_dma_c_ = event_ptr;
      is_pending_ = false;

    } else {
      return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct NicMsixSpan : public EventSpan {
  std::shared_ptr<Event> nic_msix_ = nullptr;

  NicMsixSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::nic_msix) {
  }

  ~NicMsixSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
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
    is_pending_ = false;
    return true;
  }
};

struct NicMmioSpan : public EventSpan {
  // nic action nic_mmio_w_ or nic_mmio_r_
  std::shared_ptr<Event> action_ = nullptr;
  bool is_read_ = false;

  explicit NicMmioSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::nic_mmio) {
  }

  ~NicMmioSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr) or action_) {
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

struct NicDmaSpan : public EventSpan {
  // NicDmaI_t
  std::shared_ptr<Event> dma_issue_ = nullptr;
  // NicDmaEx_t
  std::shared_ptr<Event> nic_dma_execution_ = nullptr;
  // NicDmaCW_t or NicDmaCR_t
  std::shared_ptr<Event> nic_dma_completion_ = nullptr;
  bool is_read_ = true;

  explicit NicDmaSpan(uint64_t source_id) : EventSpan(source_id, span_type::nic_dma) {
  }

  ~NicDmaSpan() = default;

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr)) {
      return false;
    }

    switch (event_ptr->get_type()) {
      case EventType::NicDmaI_t: {
        if (dma_issue_) {
          return false;
        }
        dma_issue_ = event_ptr;
        break;
      }

      case EventType::NicDmaEx_t: {
        if (not dma_issue_ or nic_dma_execution_) {
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
        if (not dma_issue_ or not nic_dma_execution_ or nic_dma_completion_) {
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

      default:return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

struct NicEthSpan : public EventSpan {
  // NicTx or NicRx
  std::shared_ptr<Event> tx_rx_ = nullptr;
  bool is_send_ = false;

  explicit NicEthSpan(uint64_t source_id) : EventSpan(source_id, span_type::nic_eth) {
  }

  ~NicEthSpan() = default;

  inline bool is_transmit() const {
    return is_send_;
  }

  inline bool is_receive() const {
    return not is_transmit();
  }

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr) or tx_rx_) {
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

struct GenericSingleSpan : public EventSpan {
  std::shared_ptr<Event> event_p_ = nullptr;

  explicit GenericSingleSpan(uint64_t source_id)
      : EventSpan(source_id, span_type::generic_single) {
  }

  bool add_to_span(std::shared_ptr<Event> event_ptr) override {
    if (not is_potential_add(event_ptr) or event_p_) {
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

inline bool is_type(std::shared_ptr<EventSpan> span, span_type type) {
  if (not span) {
    return false;
  }
  return span->get_type() == type;
}

struct SpanPrinter
    : public consumer<std::shared_ptr<EventSpan>> {
  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> resume_executor,
                                     std::shared_ptr<Channel<std::shared_ptr<EventSpan>>> &src_chan) {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(src_chan, channel_is_null);

    std::optional<std::shared_ptr<EventSpan>> next_span_opt;
    std::shared_ptr<EventSpan> next_span = nullptr;
    for (next_span_opt = co_await src_chan->pop(resume_executor); next_span_opt.has_value();
         next_span_opt = co_await src_chan->pop(resume_executor)) {
      next_span = next_span_opt.value();
      throw_if_empty(next_span, span_is_null);
      next_span->display(std::cout);
    }

    co_return;
  }

  SpanPrinter() : consumer<std::shared_ptr<EventSpan>>() {
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_PACK_H_
