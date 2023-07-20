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

#ifndef SIMBRICKS_TRACE_EVENT_SPAN_H_
#define SIMBRICKS_TRACE_EVENT_SPAN_H_

#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include <optional>
#include <mutex>

#include "util/exception.h"
#include "corobelt/corobelt.h"
#include "events/events.h"
#include "env/traceEnvironment.h"
#include "analytics/context.h"
#include "util/utils.h"

enum span_type {
  kHostCall,
  kHostMsix,
  kHostMmio,
  kHostDma,
  kHostInt,
  kHostPci,
  kNicDma,
  kNicMmio,
  kNicEth,
  kNicMsix,
  kGenericSingle
};

inline std::ostream &operator<<(std::ostream &out, span_type type) {
  switch (type) {
    case span_type::kHostCall:out << "kHostCall";
      break;
    case span_type::kHostMsix:out << "kHostMsix";
      break;
    case span_type::kHostMmio:out << "kHostMmio";
      break;
    case span_type::kHostDma:out << "kHostDma";
      break;
    case span_type::kHostInt:out << "kHostInt";
      break;
    case span_type::kHostPci:out << "kHostPci";
      break;
    case span_type::kNicDma:out << "kNicDma";
      break;
    case span_type::kNicMmio:out << "kNicMmio";
      break;
    case span_type::kNicEth:out << "kNicEth";
      break;
    case span_type::kNicMsix:out << "kNicMsix";
      break;
    case span_type::kGenericSingle:out << "kGenericSingle";
      break;
    default:out << "could not represent given span type";
      break;
  }
  return out;
}

class EventSpan {
 protected:
  uint64_t id_;
  uint64_t source_id_;
  span_type type_;
  std::vector<std::shared_ptr<Event>> events_;
  bool is_pending_ = true;
  bool is_relevant_ = false;
  std::shared_ptr<EventSpan> original_ = nullptr;
  std::shared_ptr<TraceContext> trace_context_ = nullptr;

  std::recursive_mutex span_mutex_;

  inline static const char *tc_null_ = "try setting std::shared_ptr<TraceContext> which is null";

 public:
  virtual void Display(std::ostream &out) {
    out << "id: " << std::to_string(id_);
    out << ", source_id: " << std::to_string(source_id_);
    out << ", kind: " << type_;
    if (not events_.empty()) {
      out << ", starting_event={" << events_.front() << "}";
      out << ", ending_event={" << events_.back() << "}";
    }
    // TODO: fix
    //WriteIdent(out, ident);
    //out << "has parent? " << (parent_ != nullptr) << std::endl;
    //WriteIdent(out, ident);
    //out << std::endl;
    //for (const std::shared_ptr<Event> &event : events_) {
    //  WriteIdent(out, ident);
    //  out << *event << std::endl;
    //}
  }

  void SetOriginal(const std::shared_ptr<EventSpan> &original) {
    throw_if_empty(original, "EventSpan::SetOriginal: original is empty");
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    original_ = original;
  }

  bool IsCopy() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return original_ != nullptr;
  }

  uint64_t GetOriginalId() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    throw_if_empty(original_, "EventSpan::GetOriginalId: span is not a copy");
    return original_->GetId();
  }

  size_t GetAmountEvents() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return events_.size();
  }

  std::shared_ptr<Event> GetAt(size_t index) {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    if (index >= events_.size()) {
      return nullptr;
    }
    return events_[index];
  }

  inline uint64_t GetId() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return id_;
  }

  inline span_type GetType() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return type_;
  }

  inline uint64_t GetSourceId() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return source_id_;
  }

  inline const std::shared_ptr<TraceContext> &GetContext() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return trace_context_;
  }

  virtual void MarkAsDone() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    is_pending_ = false;
  }

  bool IsPending() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_pending_;
  }

  bool IsComplete() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return not IsPending();
  }

  inline void MarkAsRelevant() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    is_relevant_ = true;
  }

  inline void MarkAsNonRelevant() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    is_relevant_ = false;
  }

  uint64_t GetStartingTs() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (events_.empty()) {
      return 0xFFFFFFFFFFFFFFFF;
    }

    const std::shared_ptr<Event> event_ptr = events_.front();
    if (event_ptr) {
      return event_ptr->GetTs();
    }
    return 0xFFFFFFFFFFFFFFFF;
  }

  uint64_t GetCompletionTs() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (events_.empty() or IsPending()) {
      return 0xFFFFFFFFFFFFFFFF;
    }

    const std::shared_ptr<Event> event_ptr = events_.back();
    if (event_ptr) {
      return event_ptr->GetTs();
    }
    return 0xFFFFFFFFFFFFFFFF;
  }

  bool SetContext(std::shared_ptr<TraceContext> traceContext, bool override_existing) {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not override_existing and trace_context_) {
      return false;
    }
    throw_if_empty(traceContext, tc_null_);

    auto parent = traceContext->GetParent();
    if (not parent or parent->GetStartingTs() >= GetStartingTs()) {
      return false;
    }

    trace_context_ = traceContext;
    return true;
  }

  bool HasParent() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    return trace_context_ != nullptr and trace_context_->GetParent() != nullptr;
  }

  std::shared_ptr<EventSpan> GetParent() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not trace_context_) {
      return nullptr;
    }

    return trace_context_->GetParent();
  }

  virtual ~EventSpan() = default;

  explicit EventSpan(std::shared_ptr<TraceContext> trace_context, uint64_t source_id, span_type type)
      : id_(TraceEnvironment::GetNextSpanId()),
        source_id_(source_id),
        type_(type),
        trace_context_(trace_context) {
    throw_if_empty(trace_context, tc_null_);
  }

  EventSpan(const EventSpan &other)
      : id_(TraceEnvironment::GetNextSpanId()),
        source_id_(other.source_id_),
        type_(other.type_),
        events_(other.events_),
        is_pending_(other.is_pending_),
        is_relevant_(other.is_relevant_),
        original_(other.original_),
        trace_context_(other.trace_context_) {
    // NOTE: we make only a shallow copy
  }

  virtual EventSpan *clone() = 0;

  virtual bool AddToSpan(std::shared_ptr<Event> event_ptr) = 0;

 protected:
  // When calling this method the lock must be held
  bool IsPotentialAdd(std::shared_ptr<Event> &event_ptr) {
    if (not event_ptr) {
      return false;
    }

    if (IsComplete()) {
      return false;
    }

    if (not events_.empty()) {
      auto first = events_.front();
      if (first->GetParserIdent() != event_ptr->GetParserIdent()) {
        return false;
      }
      // TODO: investigate this further in original log files
      // if (first->get_ts() > event_ptr->GetTs()) {
      //   return false;
      // }
    }

    return true;
  }
};

class HostCallSpan : public EventSpan {
  std::shared_ptr<Event> call_span_entry_ = nullptr;
  std::shared_ptr<Event> syscall_return_ = nullptr;
  bool kernel_transmit_ = false;
  bool driver_transmit_ = false;
  bool kernel_receive_ = false;
  bool driver_receive_ = false;
  bool is_fragmented_ = true;

 public:
  explicit HostCallSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id, bool fragmented)
      : EventSpan(trace_context, source_id, span_type::kHostCall), is_fragmented_(fragmented) {
  }

  // NOTE: we make only a shallow copy
  HostCallSpan(const HostCallSpan &other) = default;

  EventSpan *clone() override {
    return new HostCallSpan(*this);
  }

  ~HostCallSpan() override = default;

  bool DoesKernelTransmit() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return kernel_transmit_;
  }

  bool DoesDriverTransmit() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return driver_transmit_;
  }

  bool DoesKernelReceive() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return kernel_receive_;
  }

  bool DoesDriverReceive() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return driver_receive_;
  }

  bool IsOverallTx() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return kernel_transmit_ and driver_transmit_;
  }

  bool IsOverallRx() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return kernel_receive_ and driver_receive_;
  }

  bool IsFragmented() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_fragmented_;
  }

  void MarkAsDone() override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    is_fragmented_ = is_fragmented_ or call_span_entry_ == nullptr or syscall_return_ == nullptr;
    is_pending_ = false;
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      std::cout << "is not a potential add" << std::endl;
      return false;
    }

    if (not IsType(event_ptr, EventType::kHostCallT)) {
      return false;
    }

    if (TraceEnvironment::IsSysEntry(event_ptr)) {
      if (is_fragmented_ or call_span_entry_) {
        is_pending_ = false;
        syscall_return_ = events_.back();
        is_fragmented_ = false;
        return false;
      }

      is_pending_ = true;
      call_span_entry_ = event_ptr;
      events_.push_back(event_ptr);
      return true;
    }

    if (TraceEnvironment::IsKernelTx(event_ptr)) {
      kernel_transmit_ = true;
    } else if (TraceEnvironment::IsDriverTx(event_ptr)) {
      driver_transmit_ = true;
    } else if (TraceEnvironment::IsKernelRx(event_ptr)) {
      kernel_receive_ = true;
    } else if (TraceEnvironment::IsDriverRx(event_ptr)) {
      driver_receive_ = true;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

class HostIntSpan : public EventSpan {
  std::shared_ptr<Event> host_post_int_ = nullptr;
  std::shared_ptr<Event> host_clear_int_ = nullptr;

 public:
  explicit HostIntSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kHostInt) {
  }

  // NOTE: we make only a shallow copy
  HostIntSpan(const HostIntSpan &other) = default;

  EventSpan *clone() override {
    return new HostIntSpan(*this);
  }

  ~HostIntSpan() override = default;

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    if (IsType(event_ptr, EventType::kHostPostIntT)) {
      if (host_post_int_) {
        return false;
      }
      host_post_int_ = event_ptr;

    } else if (IsType(event_ptr, EventType::kHostClearIntT)) {
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

class HostDmaSpan : public EventSpan {
  // kHostDmaWT or kHostDmaRT
  std::shared_ptr<Event> host_dma_execution_ = nullptr;
  bool is_read_ = true;
  // kHostDmaCT
  std::shared_ptr<Event> host_dma_completion_ = nullptr;

 public:
  explicit HostDmaSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kHostDma) {
  }

  // NOTE: we make only a shallow copy
  HostDmaSpan(const HostDmaSpan &other) = default;

  EventSpan *clone() override {
    return new HostDmaSpan(*this);
  }

  ~HostDmaSpan() override = default;

  bool IsRead() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_read_;
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    switch (event_ptr->GetType()) {
      case EventType::kHostDmaWT:
      case EventType::kHostDmaRT: {
        if (host_dma_execution_) {
          return false;
        }

        is_read_ = IsType(event_ptr, EventType::kHostDmaRT);
        host_dma_execution_ = event_ptr;
        break;
      }

      case EventType::kHostDmaCT: {
        if (not host_dma_execution_ or host_dma_completion_) {
          return false;
        }

        auto exec =
            std::static_pointer_cast<HostAddrSizeOp>(host_dma_execution_);
        auto comp = std::static_pointer_cast<HostDmaC>(event_ptr);
        if (exec->GetId() != comp->GetId()) {
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

class HostMmioSpan : public EventSpan {
  // issue, either host_mmio_w_ or host_mmio_r_
  std::shared_ptr<Event> host_mmio_issue_ = nullptr;
  bool is_read_ = false;
  std::shared_ptr<Event> host_msi_read_resp_ = nullptr;
  bool pci_before_ = false;
  std::shared_ptr<Event> im_mmio_resp_ = nullptr;
  // completion, either host_mmio_cw_ or host_mmio_cr_
  std::shared_ptr<Event> completion_ = nullptr;

 public:
  explicit HostMmioSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id, bool pci_before)
      : EventSpan(trace_context, source_id, span_type::kHostMmio),
        pci_before_(pci_before) {
  }

  // NOTE: we make only a shallow copy
  HostMmioSpan(const HostMmioSpan &other) = default;

  EventSpan *clone() override {
    return new HostMmioSpan(*this);
  }

  ~HostMmioSpan() override = default;

  inline bool IsAfterPci() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return pci_before_;
  }

  inline bool IsRead() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_read_;
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    switch (event_ptr->GetType()) {
      case EventType::kHostMmioWT: {
        if (host_mmio_issue_) {
          return false;
        }
        is_read_ = false;
        host_mmio_issue_ = event_ptr;
        break;
      }
      case EventType::kHostMmioRT: {
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
          if (issue->GetId() != th->GetId()) {
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

      case EventType::kHostMmioImRespPoWT: {
        if (not host_mmio_issue_ or is_read_ or im_mmio_resp_) {
          return false;
        }
        if (host_mmio_issue_->GetTs() != event_ptr->GetTs()) {
          return false;
        }
        im_mmio_resp_ = event_ptr;
        break;
      }

      case EventType::kHostMmioCWT:
      case EventType::kHostMmioCRT: {
        if (IsType(event_ptr, EventType::kHostMmioCWT)) {
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
        if (issue->GetId() != comp->GetId()) {
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

class HostMsixSpan : public EventSpan {
  std::shared_ptr<Event> host_msix_ = nullptr;
  std::shared_ptr<Event> host_dma_c_ = nullptr;

 public:
  explicit HostMsixSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kHostMsix) {
  }

  // NOTE: we make only a shallow copy
  HostMsixSpan(const HostMsixSpan &other) = default;

  EventSpan *clone() override {
    return new HostMsixSpan(*this);
  }

  ~HostMsixSpan() override = default;

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    if (IsType(event_ptr, EventType::kHostMsiXT)) {
      if (host_msix_) {
        return false;
      }
      host_msix_ = event_ptr;
      is_pending_ = true;

    } else if (IsType(event_ptr, EventType::kHostDmaCT)) {
      if (not host_msix_ or host_dma_c_) {
        return false;
      }

      auto dma = std::static_pointer_cast<HostDmaC>(event_ptr);
      if (dma->GetId() != 0) {
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

class HostPciSpan : public EventSpan {
  std::shared_ptr<Event> host_pci_rw_ = nullptr;
  std::shared_ptr<Event> host_conf_rw_ = nullptr;
  bool is_read_ = false;

 public:
  explicit HostPciSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kHostPci) {
  }

  // NOTE: we make only a shallow copy
  HostPciSpan(const HostPciSpan &other) = default;

  EventSpan *clone() override {
    return new HostPciSpan(*this);
  }

  ~HostPciSpan() override = default;

  inline bool IsRead() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_read_;
  }

  inline bool IsWrite() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return not IsRead();
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    if (IsType(event_ptr, EventType::kHostPciRWT)) {
      if (host_pci_rw_) {
        return false;
      }
      host_pci_rw_ = event_ptr;
      is_pending_ = true;
      is_read_ = std::static_pointer_cast<HostPciRW>(event_ptr)->IsRead();

    } else if (IsType(event_ptr, EventType::kHostConfT)) {
      if (not host_pci_rw_ or host_conf_rw_) {
        return false;
      }
      auto conf_rw = std::static_pointer_cast<HostConf>(event_ptr);
      if (conf_rw->IsRead() != is_read_) {
        return false;
      }
      host_conf_rw_ = event_ptr;
      is_pending_ = false;

    } else {
      return false;
    }

    events_.push_back(event_ptr);
    return true;
  }
};

class NicMsixSpan : public EventSpan {
  std::shared_ptr<Event> nic_msix_ = nullptr;

 public:
  NicMsixSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kNicMsix) {
  }

  // NOTE: we make only a shallow copy
  NicMsixSpan(const NicMsixSpan &other) = default;

  EventSpan *clone() override {
    return new NicMsixSpan(*this);
  }

  ~NicMsixSpan() override = default;

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    if (IsType(event_ptr, EventType::kNicMsixT)) {
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

class NicMmioSpan : public EventSpan {
  // nic action nic_mmio_w_ or nic_mmio_r_
  std::shared_ptr<Event> action_ = nullptr;
  bool is_read_ = false;

 public:
  explicit NicMmioSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kNicMmio) {
  }

  // NOTE: we make only a shallow copy
  NicMmioSpan(const NicMmioSpan &other) = default;

  EventSpan *clone() override {
    return new NicMmioSpan(*this);
  }

  ~NicMmioSpan() override = default;

  bool IsRead() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_read_;
  }

  bool IsWrite() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return not IsRead();
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr) or action_) {
      return false;
    }

    if (IsType(event_ptr, EventType::kNicMmioRT)) {
      is_read_ = true;
    } else if (IsType(event_ptr, EventType::kNicMmioWT)) {
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

class NicDmaSpan : public EventSpan {
  // kNicDmaIT
  std::shared_ptr<Event> dma_issue_ = nullptr;
  // kNicDmaExT
  std::shared_ptr<Event> nic_dma_execution_ = nullptr;
  // kNicDmaCWT or kNicDmaCRT
  std::shared_ptr<Event> nic_dma_completion_ = nullptr;
  bool is_read_ = true;

 public:
  explicit NicDmaSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kNicDma) {
  }

  // NOTE: we make only a shallow copy
  NicDmaSpan(const NicDmaSpan &other) = default;

  EventSpan *clone() override {
    return new NicDmaSpan(*this);
  }

  ~NicDmaSpan() override = default;

  bool IsRead() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_read_;
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr)) {
      return false;
    }

    switch (event_ptr->GetType()) {
      case EventType::kNicDmaIT: {
        if (dma_issue_) {
          return false;
        }
        dma_issue_ = event_ptr;
        break;
      }

      case EventType::kNicDmaExT: {
        if (not dma_issue_ or nic_dma_execution_) {
          return false;
        }
        auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
        auto exec = std::static_pointer_cast<NicDmaEx>(event_ptr);
        if (issue->GetId() != exec->GetId() or issue->GetAddr() != exec->GetAddr()) {
          return false;
        }
        nic_dma_execution_ = event_ptr;
        break;
      }

      case EventType::kNicDmaCWT:
      case EventType::kNicDmaCRT: {
        if (not dma_issue_ or not nic_dma_execution_ or nic_dma_completion_) {
          return false;
        }

        is_read_ = IsType(event_ptr, EventType::kNicDmaCRT);

        auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
        auto comp = std::static_pointer_cast<NicDma>(event_ptr);
        if (issue->GetId() != comp->GetId() or issue->GetAddr() != comp->GetAddr()) {
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

class NicEthSpan : public EventSpan {
  // NicTx or NicRx
  std::shared_ptr<Event> tx_rx_ = nullptr;
  bool is_send_ = false;

 public:
  explicit NicEthSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kNicEth) {
  }

  // NOTE: we make only a shallow copy
  NicEthSpan(const NicEthSpan &other) = default;

  EventSpan *clone() override {
    return new NicEthSpan(*this);
  }

  ~NicEthSpan() override = default;

  inline bool IsTransmit() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return is_send_;
  }

  inline bool IsReceive() {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
    return not IsTransmit();
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr) or tx_rx_) {
      return false;
    }

    if (IsType(event_ptr, EventType::kNicTxT)) {
      is_send_ = true;
    } else if (IsType(event_ptr, EventType::kNicRxT)) {
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

class GenericSingleSpan : public EventSpan {
  std::shared_ptr<Event> event_p_ = nullptr;

 public:
  explicit GenericSingleSpan(std::shared_ptr<TraceContext> &trace_context, uint64_t source_id)
      : EventSpan(trace_context, source_id, span_type::kGenericSingle) {
  }

  // NOTE: we make only a shallow copy
  GenericSingleSpan(const GenericSingleSpan &other) = default;

  EventSpan *clone() override {
    return new GenericSingleSpan(*this);
  }

  bool AddToSpan(std::shared_ptr<Event> event_ptr) override {
    const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

    if (not IsPotentialAdd(event_ptr) or event_p_) {
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

inline bool IsType(std::shared_ptr<EventSpan> &span, span_type type) {
  if (not span) {
    return false;
  }
  return span->GetType() == type;
}

inline std::shared_ptr<EventSpan> CloneShared(const std::shared_ptr<EventSpan> &other) {
  throw_if_empty(other, span_is_null);
  auto raw_ptr = other->clone();
  throw_if_empty(raw_ptr, "EventSpan CloneShared: raw pointer is null");
  return std::shared_ptr<EventSpan>(raw_ptr);
}

inline std::string GetTypeStr(std::shared_ptr<EventSpan> &span) {
  if (not span) {
    return "";
  }
  std::stringstream sss;
  sss << span->GetType();
  return std::move(sss.str());
}

inline std::ostream &operator<<(std::ostream& out, EventSpan& span) {
  span.Display(out);
  return out;
}

inline std::ostream &operator<<(std::ostream& out, std::shared_ptr<EventSpan>& span) {
  if (span) {
    out << *span;
  } else {
    out << "null";
  }
  return out;
}

struct SpanPrinter
    : public consumer<std::shared_ptr<EventSpan>> {
  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> resume_executor,
                                     std::shared_ptr<Channel<std::shared_ptr<EventSpan>>> &src_chan) override {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(src_chan, channel_is_null);

    std::optional<std::shared_ptr<EventSpan>> next_span_opt;
    std::shared_ptr<EventSpan> next_span = nullptr;
    for (next_span_opt = co_await src_chan->Pop(resume_executor); next_span_opt.has_value();
         next_span_opt = co_await src_chan->Pop(resume_executor)) {
      next_span = next_span_opt.value();
      throw_if_empty(next_span, span_is_null);
      std::cout << next_span << std::endl;
    }

    co_return;
  }

  SpanPrinter() : consumer<std::shared_ptr<EventSpan>>() {
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_SPAN_H_
