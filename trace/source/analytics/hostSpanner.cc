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

#include <cassert>

#include "analytics/spanner.h"
#include "util/exception.h"

concurrencpp::lazy_result<void>
HostSpanner::FinishPendingSpan(std::shared_ptr<concurrencpp::executor> resume_executor) {
  if (not pending_host_call_span_) {
    co_return;
  }

  if (not pending_host_call_span_->DoesKernelReceive()) {
    tracer_.MarkSpanAsDone(name_, pending_host_call_span_);
    co_return;
  }

  //std::cout << "host tryna eceive receive update. Current queue: " << std::endl;
  //from_nic_receives_queue_->Display(resume_executor, std::cout);

  //std::cout << "host try poll nic receive" << std::endl;
  auto context_opt = co_await from_nic_receives_queue_->Pop(resume_executor);
  //std::cout << "host polled nic receive" << std::endl;
  auto context = OrElseThrow(context_opt,
                             "HostSpanner::CreateTraceStartingSpan could not receive rx context");
  tracer_.AddParentLazily(pending_host_call_span_, context->GetNonEmptyParent());

  uint64_t syscall_start = pending_host_call_span_->GetStartingTs();
  std::function<bool(std::shared_ptr<Context> &)>
      did_arrive_before_receive_syscall = [&syscall_start](std::shared_ptr<Context> &context) {
    return context->HasParent() and syscall_start > context->GetParent()->GetStartingTs();
  };

  while (context_opt) {
    context_opt = co_await from_nic_receives_queue_->TryPopOnTrue(resume_executor, did_arrive_before_receive_syscall);
    if (not context_opt.has_value()) {
      break;
    }
    context = context_opt.value();

    auto copy_span = CloneShared(pending_host_call_span_);
    copy_span->SetOriginal(pending_host_call_span_);
    tracer_.StartSpanSetParentContext(copy_span, context->GetNonEmptyParent());
    tracer_.MarkSpanAsDone(name_, copy_span);
  }

  tracer_.MarkSpanAsDone(name_, pending_host_call_span_);
  co_return;
}

concurrencpp::lazy_result<bool>
HostSpanner::CreateTraceStartingSpan(std::shared_ptr<concurrencpp::executor> resume_executor,
                                     std::shared_ptr<Event> &starting_event,
                                     bool fragmented) {
  co_await FinishPendingSpan(resume_executor);
  pending_host_call_span_ =
      tracer_.StartSpan<HostCallSpan>(starting_event, starting_event->GetParserIdent(), fragmented);
  throw_if_empty(pending_host_call_span_, "could not register new pending_host_call_span_");
  last_trace_starting_span_ = pending_host_call_span_;

  found_transmit_ = false;
  found_receive_ = false;
  expected_xmits_ = 0;
  pci_write_before_ = false;
  co_return true;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelCall(std::shared_ptr<concurrencpp::executor> resume_executor,
                        std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_call_span_ and not co_await CreateTraceStartingSpan(resume_executor, event_ptr, false)) {
    co_return false;
  }

  //if (pending_host_call_span_) {
  //  if (found_transmit_ and TraceEnvironment::IsKernelOrDriverRx(event_ptr)) {
  //    auto created = CreateTraceStartingSpan(event_ptr, true);
  //    assert(created and "HostSpanner: CreateTraceStartingSpan could not create new span");
  //  }
  //
  //  if (found_receive_ and TraceEnvironment::IsKernelOrDriverTx(event_ptr)) {
  //    auto created = CreateTraceStartingSpan(event_ptr, true);
  //    assert(created and "HostSpanner: CreateTraceStartingSpan could not create new span");
  //  }
  //}

  throw_if_empty(pending_host_call_span_, span_is_null);
  if (pending_host_call_span_->AddToSpan(event_ptr)) {
    pci_write_before_ = TraceEnvironment::is_pci_write(event_ptr);

    if (pending_host_call_span_->DoesDriverTransmit()) {
      found_transmit_ = true;
    } else if (pending_host_call_span_->DoesKernelReceive()) {
      found_receive_ = true;
    }
    co_return true;

  } else if (pending_host_call_span_->IsComplete()) {
    auto created_new = co_await CreateTraceStartingSpan(resume_executor, event_ptr, false);
    assert (created_new and "HostSpanner: CreateTraceStartingSpan could not create new span");

    if (not pending_host_call_span_) {
      std::cerr << "found new syscall entry, could not add "
                   "pending_host_call_span_"
                << std::endl;
      co_return false;
    }

    co_return true;
  }

  co_return false;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelMmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                        std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto pending_mmio_span = iterate_add_erase<HostMmioSpan>(pending_host_mmio_spans_, event_ptr);
  if (pending_mmio_span) {
    if (pending_mmio_span->IsComplete()) {
      tracer_.MarkSpanAsDone(name_, pending_mmio_span);
    }

    co_return true;
  }

  if (is_type(event_ptr, EventType::kHostMmioWT)) {
    pending_mmio_span = nullptr;
    for (auto it = pending_host_mmio_spans_.begin(); it != pending_host_mmio_spans_.end(); it++) {
      if ((*it)->IsAfterPci()) {
        pending_mmio_span = *it;
        pending_host_mmio_spans_.erase(it);
        break;
      }
    }

    if (pending_mmio_span) {
      tracer_.MarkSpanAsDone(name_, pending_mmio_span);
    }
    pending_mmio_span = nullptr;
  }

  if (not pending_mmio_span) {
    // create a pack that belongs to the trace of the current host call span
    pending_mmio_span = tracer_.StartSpanByParent<HostMmioSpan>(pending_host_call_span_,
                                                                event_ptr, event_ptr->GetParserIdent(),
                                                                pci_write_before_);
    if (not pending_mmio_span) {
      co_return false;
    }

    assert(is_type(event_ptr, EventType::kHostMmioWT) or is_type(event_ptr, EventType::kHostMmioRT)
               and "try to create mmio host span but event is neither read nor write");

    if (not pci_write_before_) {
      //std::cout << "host try push mmio" << std::endl;
      auto context = create_shared<Context>("HandelMmio could not create context",
                                            expectation::kMmio, pending_mmio_span);
      if (not co_await to_nic_queue_->Push(resume_executor, context)) {
        std::cerr << "could not push to nic that mmio is expected"
                  << std::endl;
        // TODO: error
        // note: we will not return false as the span creation itself id work
      }
      //std::cout << "host pushed mmio" << std::endl;
    }

    pending_host_mmio_spans_.push_back(pending_mmio_span);
    co_return true;
  }

  co_return false;
}

concurrencpp::lazy_result<bool> HostSpanner::HandelPci(std::shared_ptr<concurrencpp::executor> resume_executor,
                                                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");
  if (pending_pci_spans_) {
    assert(is_type(event_ptr, EventType::kHostConfT)
               and "HostSpanner::HandelPci: found second pci action in flight");
    bool could_be_added = pending_pci_spans_->AddToSpan(event_ptr);
    throw_on(not could_be_added, "HostSpanner::HandelPci: could not add event to pending pci span");
    assert(pending_pci_spans_->IsComplete() and "HostSpanner::HandelPci: span is not complete but should be");
    tracer_.MarkSpanAsDone(name_, pending_pci_spans_);
    co_return true;
  }

  assert(is_type(event_ptr, EventType::kHostPciRWT)
             and "HostSpanner::HandelPci: event is no pci starting event");
  auto new_pci_span =
      tracer_.StartSpanByParent<HostPciSpan>(pending_host_call_span_, event_ptr, event_ptr->GetParserIdent());
  if (not new_pci_span) {
    co_return false;
  }
  pending_pci_spans_ = new_pci_span;

  auto context = create_shared<Context>("HostSpanner::HandelPci could not create context",
                                        expectation::kMmio, pending_pci_spans_);
  //std::cout << "host try push mmio pci" << std::endl;
  bool could_push = co_await to_nic_queue_->Push(resume_executor, context);
  throw_on(not could_push, "HostSpanner::HandelPci could not to nic that mmio is expected due to pci");
  //std::cout << "host pushed pci" << std::endl;

  co_return true;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  // check if we had an interrupt (msix) this belongs to
  if (pending_host_msix_span_ and pending_host_msix_span_->AddToSpan(event_ptr)) {
    assert(pending_host_msix_span_->IsComplete() and "pending_host_msix_span_ is not complete");
    tracer_.MarkSpanAsDone(name_, pending_host_msix_span_);
    pending_host_msix_span_ = nullptr;
    co_return true;
  }

  auto pending_dma =
      iterate_add_erase<HostDmaSpan>(pending_host_dma_spans_,
                                     event_ptr);
  if (pending_dma) {
    if (pending_dma->IsComplete()) {
      tracer_.MarkSpanAsDone(name_, pending_dma);
    }
    co_return true;
  }

  // when receiving a dma, we expect to get a context from the nic simulator,
  // hence poll this context blocking!!
  //std::cout << "host try poll dma" << std::endl;
  auto con_opt = co_await from_nic_queue_->Pop(resume_executor);
  throw_on(not con_opt.has_value(), context_is_null);
  auto con = con_opt.value();
  //std::cout << "host polled dma" << std::endl;
  if (not is_expectation(con_opt.value(), expectation::kDma)) {
    std::cerr << "when polling for dma context, no dma context was fetched"
              << std::endl;
    co_return false;
  }

  // TODO: investigate this case further
  if (is_type(event_ptr, EventType::kHostDmaCT)) {
    std::cerr << "unexpected event: " << *event_ptr << std::endl;
    co_return false;
  }
  assert(not is_type(event_ptr, EventType::kHostDmaCT) and "cannot start HostDmaSPan with Dma completion");
  pending_dma = tracer_.StartSpanByParent<HostDmaSpan>(con->GetNonEmptyParent(),
                                                       event_ptr, event_ptr->GetParserIdent());
  if (not pending_dma) {
    co_return false;
  }
  pending_host_dma_spans_.push_back(pending_dma);
  co_return true;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelMsix(std::shared_ptr<concurrencpp::executor> resume_executor,
                        std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  //std::cout << "host try poll msix" << std::endl;
  auto con_opt = co_await from_nic_queue_->Pop(resume_executor);
  throw_on(not con_opt.has_value(), context_is_null);
  auto con = con_opt.value();
  //std::cout << "host polled msix" << std::endl;
  if (not is_expectation(con, expectation::kMsix)) {
    std::cerr << "did not receive msix on context queue" << std::endl;
    co_return false;
  }

  pending_host_msix_span_ =
      tracer_.StartSpanByParent<HostMsixSpan>(con->GetNonEmptyParent(),
                                              event_ptr, event_ptr->GetParserIdent());
  if (not pending_host_msix_span_) {
    co_return false;
  }

  assert(pending_host_msix_span_->IsPending() and "host msix span is complete");
  co_return true;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelInt([[maybe_unused]] std::shared_ptr<concurrencpp::executor> resume_executor,
                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_int_span_) {
    pending_host_int_span_ =
        tracer_.StartSpanByParent<HostIntSpan>(pending_host_call_span_,
                                               event_ptr, event_ptr->GetParserIdent());
    if (not pending_host_int_span_) {
      co_return false;
    }

    co_return true;
  }

  if (not pending_host_int_span_->AddToSpan(event_ptr)) {
    co_return false;
  }

  if (pending_host_int_span_->IsPending()) {
    co_return false;
  }

  tracer_.MarkSpanAsDone(name_, pending_host_int_span_);
  pending_host_int_span_ = nullptr;
  co_return true;
}

HostSpanner::HostSpanner(std::string &&name, Tracer &tra, Timer &timer,
                         std::shared_ptr<Channel<std::shared_ptr<Context>>> to_nic,
                         std::shared_ptr<Channel<std::shared_ptr<Context>>> from_nic,
                         std::shared_ptr<Channel<std::shared_ptr<Context>>> from_nic_receives,
                         bool is_client)
    : Spanner(std::move(name), tra, timer),
      to_nic_queue_(to_nic),
      from_nic_queue_(from_nic),
      from_nic_receives_queue_(from_nic_receives),
      is_client_(is_client) {
  throw_if_empty(to_nic, queue_is_null);
  throw_if_empty(from_nic, queue_is_null);

  auto handel_call = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelCall(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostCallT, handel_call);

  auto handel_mmio = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelMmio(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostMmioWT, handel_mmio);
  RegisterHandler(EventType::kHostMmioRT, handel_mmio);
  RegisterHandler(EventType::kHostMmioImRespPoWT, handel_mmio);
  RegisterHandler(EventType::kHostMmioCWT, handel_mmio);
  RegisterHandler(EventType::kHostMmioCRT, handel_mmio);

  auto handel_pci = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelPci(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostPciRWT, handel_pci);
  RegisterHandler(EventType::kHostConfT, handel_pci);

  auto handel_dma = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelDma(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostDmaWT, handel_dma);
  RegisterHandler(EventType::kHostDmaRT, handel_dma);
  RegisterHandler(EventType::kHostDmaCT, handel_dma);

  auto handel_misx = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelMsix(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostMsiXT, handel_misx);

  auto handel_int = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelInt(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kHostPostIntT, handel_int);
  RegisterHandler(EventType::kHostClearIntT, handel_int);
}

