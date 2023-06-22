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

bool HostSpanner::CreateTraceStartingSpan(std::shared_ptr<Event> &starting_event) {
  //if (pending_host_call_span_ and is_client_) {
  // Note: this is to inform potential server hosts, that the trace is now
  // done, hence no new spans are added to it!

  // TODO: is this correcet?!?!!?!?!
  //auto marked = tracer_.MarkTraceAsDone(
  //    pending_host_call_span_->get_trace_id());
  //throw_on(not marked, "client could not mark trace as done");
  //}

  if (pending_host_call_span_) {
    tracer_.MarkSpanAsDone(pending_host_call_span_);
  }
  pending_host_call_span_ = tracer_.StartSpan<HostCallSpan>(
      name_, starting_event, starting_event->get_parser_ident());
  //pending_host_call_span_ =
  //    tracer_.rergister_new_trace<HostCallSpan>(parser_id);
  throw_if_empty(pending_host_call_span_, "could not register new pending_host_call_span_");

  found_transmit_ = false;
  found_receive_ = false;
  expected_xmits_ = 0;
  pci_write_before_ = false;
  return true;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelCall(std::shared_ptr<concurrencpp::executor> resume_executor,
                        std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_call_span_ and not CreateTraceStartingSpan(event_ptr)) {
    co_return false;
  }

  throw_if_empty(pending_host_call_span_, span_is_null);
  if (pending_host_call_span_->AddToSpan(event_ptr)) {
    pci_write_before_ = trace_environment::is_pci_write(event_ptr);

    if (trace_environment::is_nw_interface_send(event_ptr)) {
      ++expected_xmits_;
      found_transmit_ = true;
    } else if (trace_environment::is_nw_interface_receive(event_ptr)) {
      found_receive_ = true;
    }
    co_return true;

  } else if (pending_host_call_span_->IsComplete()) {
    // create a completely new trace
    if (found_receive_ and found_transmit_) {
      auto created_new = CreateTraceStartingSpan(event_ptr);
      throw_on(not created_new, "found new syscall entry, could not allocate pending_host_call_span_");

    } else if ((found_receive_ and not found_transmit_)
        or (not found_receive_ and found_transmit_)) { // get new pack for current trace
      //pending_host_call_span_ =
      //    tracer_.rergister_new_span_by_parent<HostCallSpan>(
      //        pending_host_call_span_, event_ptr->get_parser_ident());

      tracer_.MarkSpanAsDone(pending_host_call_span_);
      pending_host_call_span_ = tracer_.StartSpanByParent<HostCallSpan>(
          name_, pending_host_call_span_, event_ptr, event_ptr->get_parser_ident());
      throw_if_empty(pending_host_call_span_, "could not register new pending_host_call_span_");

    } else { // create new trace on its own
      auto created = CreateTraceStartingSpan(event_ptr);
      throw_on(not created, "found new syscall entry, could not allocate pending_host_call_span_");
    }

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
    // as the nic receives his events before this span will
    // be completed, we indicate to the nic that a mmiow a.k.a send is expected

    // TODO: expect xmit???
    //if (pending_mmio_span->IsComplete()) {
    //  // if it is a write after xmit, we inform the nic packer that we expect a
    //  // transmit
    //  if (pending_mmio_span->is_write() and expected_xmits_ > 0 and
    //      not pending_mmio_span->is_after_pci_msix_desc_addr()) {
    //    if (co_await queue_.push(resume_executor, this->GetId(), expectation::mmio,
    //                             pending_mmio_span)) {
    //      --expected_xmits_;
    //    } else {
    //      std::cerr
    //          << "unable to inform nic spanner of mmio write that shall ";
    //      std::cerr << "cause a send" << std::endl;
    //    }
    //  }
    //}

    if (pending_mmio_span->IsComplete()) {
      tracer_.MarkSpanAsDone(pending_mmio_span);
    }

    co_return true;
  }

  if (is_type(event_ptr, EventType::HostMmioW_t)) {
    pending_mmio_span = nullptr;
    for (auto it = pending_host_mmio_spans_.begin(); it != pending_host_mmio_spans_.end(); it++) {
      if ((*it)->IsAfterPci()) {
        pending_mmio_span = *it;
        pending_host_mmio_spans_.erase(it);
        break;
      }
    }

    if (pending_mmio_span) {
      //pending_mmio_span->MarkAsDone();
      tracer_.MarkSpanAsDone(pending_mmio_span);
    }
    pending_mmio_span = nullptr;
  }

  if (not pending_mmio_span) {
    // create a pack that belongs to the trace of the current host call span
    //pending_mmio_span =
    //    tracer_.rergister_new_span_by_parent<HostMmioSpan>(
    //        pending_host_call_span_, event_ptr->get_parser_ident(),
    //        pci_write_before_);
    pending_mmio_span = tracer_.StartSpanByParent<HostMmioSpan>(name_, pending_host_call_span_,
                                                                event_ptr, event_ptr->get_parser_ident(),
                                                                pci_write_before_);
    if (not pending_mmio_span) {
      co_return false;
    }

    assert(is_type(event_ptr, EventType::HostMmioW_t) or is_type(event_ptr, EventType::HostMmioR_t)
               and "try to create mmio host span but event is neither read nor write");

    if (not pci_write_before_) {
      std::cout << "host try push mmio" << std::endl;
      auto context = create_shared<Context>("HandelMmio could not create context",
                                            expectation::kMmio, pending_mmio_span->GetContext());
      if (not co_await to_nic_queue_->push(resume_executor, context)) {
        std::cerr << "could not push to nic that mmio is expected"
                  << std::endl;
        // note: we will not return false as the span creation itself id work
      }
      std::cout << "host pushed mmio" << std::endl;
    }

    pending_host_mmio_spans_.push_back(pending_mmio_span);
    co_return true;
  }

  co_return false;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  // check if we had an interrupt (msix) this belongs to
  if (pending_host_msix_span_ and pending_host_msix_span_->AddToSpan(event_ptr)) {
    assert(pending_host_msix_span_->IsComplete() and "pending_host_msix_span_ is not complete");
    tracer_.MarkSpanAsDone(pending_host_msix_span_);
    pending_host_msix_span_ = nullptr;
    co_return true;
  }

  auto pending_dma =
      iterate_add_erase<HostDmaSpan>(pending_host_dma_spans_,
                                     event_ptr);
  if (pending_dma) {
    if (pending_dma->IsComplete()) {
      tracer_.MarkSpanAsDone(pending_dma);
    }
    co_return true;
  }

  // when receiving a dma, we expect to get a context from the nic simulator,
  // hence poll this context blocking!!
  std::cout << "host try poll dma" << std::endl;
  auto con_opt = co_await from_nic_queue_->pop(resume_executor);
  throw_on(not con_opt.has_value(), context_is_null);
  auto con = con_opt.value();
  std::cout << "host polled dma" << std::endl;
  if (not is_expectation(con_opt.value(), expectation::kDma)) {
    std::cerr << "when polling for dma context, no dma context was fetched"
              << std::endl;
    co_return false;
  }

  //pending_dma = tracer_.rergister_new_span_by_parent<HostDmaSpan>(
  //    con->parent_span_, event_ptr->get_parser_ident());
  // TODO: investigate this case further
  if (is_type(event_ptr, EventType::HostDmaC_t)) {
    std::cerr << "unexpected event: " << *event_ptr << std::endl;
    co_return false;
  }
  assert(not is_type(event_ptr, EventType::HostDmaC_t) and "cannot start HostDmaSPan with Dma completion");
  auto context = con->GetNonEmptyTraceContext();
  pending_dma = tracer_.StartSpanByParent<HostDmaSpan>(name_, context->GetParent(),
                                                       event_ptr, event_ptr->get_parser_ident());
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

  std::cout << "host try poll msix" << std::endl;
  auto con_opt = co_await from_nic_queue_->pop(resume_executor);
  throw_on(not con_opt.has_value(), context_is_null);
  auto con = con_opt.value();
  std::cout << "host polled msix" << std::endl;
  if (not is_expectation(con, expectation::kMsix)) {
    std::cerr << "did not receive msix on context queue" << std::endl;
    co_return false;
  }

  //pending_host_msix_span_ = tracer_.rergister_new_span_by_parent<HostMsixSpan>(
  //    con->parent_span_, event_ptr->get_parser_ident());
  auto context = con->GetNonEmptyTraceContext();
  pending_host_msix_span_ =
      tracer_.StartSpanByParent<HostMsixSpan>(name_, context->GetParent(),
                                              event_ptr, event_ptr->get_parser_ident());
  if (not pending_host_msix_span_) {
    co_return false;
  }

  assert(pending_host_msix_span_->IsPending() and "host msix span is complete");
  co_return true;
}

concurrencpp::lazy_result<bool>
HostSpanner::HandelInt(std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_int_span_) {
    //pending_host_int_span_ =
    //    pending_host_call_span_, event_ptr->get_parser_ident());
    pending_host_int_span_ =
        tracer_.StartSpanByParent<HostIntSpan>(name_, pending_host_call_span_,
                                               event_ptr, event_ptr->get_parser_ident());
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

  tracer_.MarkSpanAsDone(pending_host_int_span_);
  pending_host_int_span_ = nullptr;
  co_return true;
}

concurrencpp::result<void> HostSpanner::consume(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan) {
  throw_if_empty(resume_executor, resume_executor_null);
  throw_if_empty(src_chan, channel_is_null);

  std::shared_ptr<Event> event_ptr = nullptr;
  bool added = false;
  std::optional<std::shared_ptr<Event>> event_ptr_opt;

  for (event_ptr_opt = co_await src_chan->pop(resume_executor); event_ptr_opt.has_value();
       event_ptr_opt = co_await src_chan->pop(resume_executor)) {
    event_ptr = event_ptr_opt.value();
    throw_if_empty(event_ptr, event_is_null);

    added = false;

    std::cout << "host spanner try handel: " << *event_ptr << std::endl;

    switch (event_ptr->get_type()) {
      case EventType::HostCall_t: {
        added = co_await HandelCall(resume_executor, event_ptr);
        break;
      }

      case EventType::HostMmioW_t:
      case EventType::HostMmioR_t:
      case EventType::HostMmioImRespPoW_t:
      case EventType::HostMmioCW_t:
      case EventType::HostMmioCR_t: {
        added = co_await HandelMmio(resume_executor, event_ptr);
        break;
      }

      case EventType::HostDmaW_t:
      case EventType::HostDmaR_t:
      case EventType::HostDmaC_t: {
        added = co_await HandelDma(resume_executor, event_ptr);
        break;
      }

      case EventType::HostMsiX_t: {
        added = co_await HandelMsix(resume_executor, event_ptr);
        break;
      }

      case EventType::HostPostInt_t:
      case EventType::HostClearInt_t: {
        added = co_await HandelInt(event_ptr);
        break;
      }

      default:std::cout << "encountered non expected event ";
        std::cout << *event_ptr << std::endl;
        added = false;
        break;
    }

    if (not added) {
      std::cout << "found event that could not be added to a pack: "
                << *event_ptr << std::endl;
    }
  }

  co_return;
}
