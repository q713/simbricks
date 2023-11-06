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

#include "analytics/span.h"

void EventSpan::Display(std::ostream &out) {
  const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
  out << "id: " << std::to_string(id_);
  out << ", source_id: " << std::to_string(source_id_);
  out << ", kind: " << type_;
  if (not events_.empty()) {
    out << ", starting_event={" << events_.front() << "}";
    out << ", ending_event={" << events_.back() << "}";
  }
  out << ", has parent? " << BoolToString(HasParent());
  out << ", parent_id=" << GetParentId();
}

uint64_t EventSpan::GetStartingTs() {
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

uint64_t EventSpan::GetCompletionTs() {
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

bool EventSpan::SetContext(const std::shared_ptr<TraceContext> &traceContext, bool override_existing) {
  const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

  if (not override_existing and trace_context_) {
    return false;
  }
  throw_if_empty(traceContext, tc_null_, source_loc::current());

  if (not traceContext->HasParent()) {
    return false;
  }
  assert(traceContext->GetParentId() != 0);
  if (traceContext->GetParentStartingTs() >= GetStartingTs()) {
    return false;
  }

  trace_context_ = traceContext;
  return true;
}

uint64_t EventSpan::GetParentId() {
  const std::lock_guard<std::recursive_mutex> guard(span_mutex_);
  if (trace_context_ == nullptr) {
    return 0; // invalid id
  }
  if (not trace_context_->HasParent()) {
    return 0; // invalid id
  }
  return trace_context_->GetParentId();
}

// When calling this method the lock must be held
bool EventSpan::IsPotentialAdd(std::shared_ptr<Event> &event_ptr) {
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

bool HostCallSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

  if (not IsPotentialAdd(event_ptr)) {
    std::cout << "is not a potential add" << '\n';
    return false;
  }

  if (not IsType(event_ptr, EventType::kHostCallT)) {
    return false;
  }

  if (trace_environment_.IsSysEntry(event_ptr)) {
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

  if (trace_environment_.IsKernelTx(event_ptr)) {
    kernel_transmit_ = true;
  } else if (trace_environment_.IsDriverTx(event_ptr)) {
    driver_transmit_ = true;
  } else if (trace_environment_.IsKernelRx(event_ptr)) {
    kernel_receive_ = true;
  } else if (trace_environment_.IsDriverRx(event_ptr)) {
    driver_receive_ = true;
  }

  events_.push_back(event_ptr);
  return true;
}

bool HostIntSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool HostDmaSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool HostMmioSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

  if (not IsPotentialAdd(event_ptr)) {
    return false;
  }

  switch (event_ptr->GetType()) {
    case EventType::kHostMmioWT:
    case EventType::kHostMmioRT: {
      if (host_mmio_issue_) {
        return false;
      }

      std::shared_ptr<HostMmioOp> mmio;
      if (IsType(event_ptr, EventType::kHostMmioRT)) {
        is_read_ = true;
        mmio = std::static_pointer_cast<HostMmioOp>(event_ptr);
      } else {
        is_read_ = false;
        auto mmiow = std::static_pointer_cast<HostMmioW>(event_ptr);
        is_posted_ = mmiow->IsPosted();
        mmio = mmiow;
      }
      bar_number_ = mmio->GetBar();
      host_mmio_issue_ = event_ptr;

      if (is_read_ and trace_environment_.IsMsixNotToDeviceBarNumber(bar_number_)) {
        is_pending_ = false;
      }

      break;
    }

    case EventType::kHostMmioImRespPoWT: {
      if (not host_mmio_issue_ or is_read_ or im_mmio_resp_ or not is_posted_) {
        return false;
      }
      if (host_mmio_issue_->GetTs() != event_ptr->GetTs()) {
        return false;
      }
      im_mmio_resp_ = event_ptr;

      if (is_posted_ or
          trace_environment_.IsMsixNotToDeviceBarNumber(bar_number_)) {
        is_pending_ = false;
      }

      break;
    }

    case EventType::kHostMmioCWT:
    case EventType::kHostMmioCRT: {
      if (trace_environment_.IsMsixNotToDeviceBarNumber(bar_number_)) {
        return false;
      }

      if (not host_mmio_issue_) {
        return false;
      }

      if (IsType(event_ptr, EventType::kHostMmioCWT)) {
        if (is_read_ or im_mmio_resp_) {
          return false;
        }
      } else {
        if (not is_read_) {
          return false;
        }
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

bool HostMsixSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool HostPciSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool NicMsixSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool NicMmioSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool NicDmaSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool NicEthSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
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

bool NetDeviceSpan::IsConsistent(const std::shared_ptr<NetworkEvent> &event_a,
                                 const std::shared_ptr<NetworkEvent> &event_b) {
  if ((event_a->HasEthernetHeader() and not event_b->HasEthernetHeader())
      or (not event_a->HasEthernetHeader() and event_b->HasEthernetHeader())) {
    return false;
  }
  if (event_a->HasEthernetHeader() and event_a->GetEthernetHeader() != event_b->GetEthernetHeader()) {
    return false;
  }

  if ((event_a->HasIpHeader() and not event_b->HasIpHeader())
      or (not event_a->HasIpHeader() and event_b->HasIpHeader())) {
    return false;
  }
  if (event_a->HasIpHeader() and event_a->GetIpHeader() != event_b->GetIpHeader()) {
    return false;
  }

  return event_a->GetNode() == event_b->GetNode() and event_a->GetDevice() == event_b->GetDevice()
      and event_a->GetDeviceType() == event_b->GetDeviceType()
      and event_a->GetPayloadSize() == event_b->GetPayloadSize();
}

bool NetDeviceSpan::AddToSpan(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::recursive_mutex> guard(span_mutex_);

  if (not IsPotentialAdd(event_ptr)) {
    return false;
  }

  switch (event_ptr->GetType()) {
    case EventType::kNetworkEnqueueT: {
      if (enqueue_ or dequeue_ or drop_) {
        return false;
      }
      auto network_event = std::static_pointer_cast<NetworkEvent>(event_ptr);
      device_type_ = network_event->GetDeviceType();
      enqueue_ = event_ptr;
      break;
    }
    case EventType::kNetworkDequeueT: {
      if (not enqueue_ or dequeue_ or drop_) {
        return false;
      }
      auto enq = std::static_pointer_cast<NetworkEvent>(enqueue_);
      auto deq = std::static_pointer_cast<NetworkEvent>(event_ptr);
      if (not IsConsistent(enq, deq)) {
        return false;
      }
      dequeue_ = event_ptr;
      is_pending_ = false;
      break;
    }
    case EventType::kNetworkDropT: {
      if (not enqueue_ or dequeue_ or drop_) {
        return false;
      }
      auto enq = std::static_pointer_cast<NetworkEvent>(enqueue_);
      auto dro = std::static_pointer_cast<NetworkEvent>(event_ptr);
      if (not IsConsistent(enq, dro)) {
        return false;
      }
      drop_ = event_ptr;
      is_pending_ = false;
      break;
    }
    default:return false;
  }

  events_.push_back(event_ptr);
  return true;
}
