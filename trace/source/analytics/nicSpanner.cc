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
#include <functional>
#include <iostream>
#include <memory>
#include <ostream>

#include "analytics/spanner.h"
#include "util/exception.h"
#include "events/printer.h"

concurrencpp::lazy_result<bool> NicSpanner::HandelMmio(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  spdlog::info("{} nic try poll mmio", name_);
  auto con_opt = co_await from_host_queue_->Pop(resume_executor);
  auto con = OrElseThrow(con_opt, TraceException::kContextIsNull, source_loc::current());
  throw_if_empty(con, TraceException::kContextIsNull, source_loc::current());
  spdlog::info("{} nic polled mmio", name_);

  if (not is_expectation(con, expectation::kMmio)) {
    std::cerr << "nic_spanner: could not poll mmio context" << '\n';
    co_return false;
  }

  auto mmio_span = tracer_.StartSpanByParentPassOnContext<NicMmioSpan>(
      con, event_ptr, event_ptr->GetParserIdent(), name_);
  if (not mmio_span) {
    std::cerr << "could not register mmio_span" << '\n';
    co_return false;
  }

  assert(mmio_span->IsComplete() and "mmio span is not complete");
  tracer_.MarkSpanAsDone(mmio_span);
  if (mmio_span->IsWrite()) {
    last_causing_ = mmio_span;
  }
  co_return true;
}

concurrencpp::lazy_result<bool> NicSpanner::HandelDma(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto pending_dma =
      iterate_add_erase<NicDmaSpan>(pending_nic_dma_spans_, event_ptr);
  if (pending_dma) {
    if (pending_dma->IsComplete()) {
      tracer_.MarkSpanAsDone(pending_dma);
    } else if (IsType(event_ptr, EventType::kNicDmaExT)) {
      // indicate to host that we expect a dma action
      spdlog::info("{} nic try push dma: {}", name_, *event_ptr);
      auto context = Context::CreatePassOnContext<expectation::kDma>(pending_dma);
      throw_on(not co_await to_host_queue_->Push(resume_executor, context),
               TraceException::kCouldNotPushToContextQueue,
               source_loc::current());
      spdlog::info("{} nic pushed dma", name_);
    }

    co_return true;
  }

  if (not IsType(event_ptr, EventType::kNicDmaIT)) {
    std::cout << "NicSpanner::HandelDma: Found non start Dma event, but neeed "
                 "to start"
              << '\n';
    co_return false;
  }

  assert(IsType(event_ptr, EventType::kNicDmaIT) and
      "try starting a new dma span with NON issue");

  // TODO: maybe we need to write at this point into the queue
  throw_if_empty(last_causing_, TraceException::kSpanIsNull, source_loc::current());
  pending_dma = tracer_.StartSpanByParent<NicDmaSpan>(
      last_causing_, event_ptr, event_ptr->GetParserIdent(), name_);
  if (not pending_dma) {
    std::cerr << "could not register new pending dma action" << '\n';
    co_return false;
  }

  pending_nic_dma_spans_.push_back(pending_dma);
  co_return true;
}

concurrencpp::lazy_result<bool> NicSpanner::HandelTxrx(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  std::shared_ptr<NicEthSpan> eth_span;
  std::shared_ptr<EventSpan> parent = nullptr;
  if (IsType(event_ptr, EventType::kNicTxT)) {
    parent = last_causing_;

    throw_if_empty(parent, TraceException::kSpanIsNull, source_loc::current());
    eth_span = tracer_.StartSpanByParent<NicEthSpan>(
        parent, event_ptr, event_ptr->GetParserIdent(), name_);

    spdlog::info("{} NicSpanner::HandelTxrx: trying to push tx context to other side", name_);
    auto context = Context::CreatePassOnContext<expectation::kRx>(eth_span);
    throw_on(not co_await to_network_queue_->Push(resume_executor, context),
             "HandelTxrx: could not write network context ",
             source_loc::current());
    spdlog::info("{} NicSpanner::HandelTxrx: pushed tx context to other side", name_);

  } else if (IsType(event_ptr, EventType::kNicRxT)) {
    std::cout << name_ << " NicSpanner::HandelTxrx: trying to pull tx context from other side" << std::endl;
    auto con_opt = co_await from_network_queue_->Pop(resume_executor);
    std::cout << name_ << " NicSpanner::HandelTxrx: pulled tx context from other side" << std::endl;
    auto con = OrElseThrow(con_opt, TraceException::kContextIsNull, source_loc::current());
    throw_on(not is_expectation(con, expectation::kRx),
             "nic_spanner: received non kRx context", source_loc::current());
    eth_span = tracer_.StartSpanByParentPassOnContext<NicEthSpan>(
        con, event_ptr, event_ptr->GetParserIdent(), name_);
    last_causing_ = eth_span;

    spdlog::info("{} nic tryna push receive update.", name_);
    auto receive_context = Context::CreatePassOnContext<expectation::kRx>(eth_span);
    const bool could_push = co_await to_host_receives_->Push(resume_executor, receive_context);
    throw_on_false(could_push,
                   "NicSpanner::HandelTxrx: could not write host receive context ",
                   source_loc::current());
    spdlog::info("{} nic pushed receive update.", name_);

  } else {
    std::cerr << "NicSpanner::HandelTxrx: unknown event type" << '\n';
    co_return false;
  }

  assert(eth_span and "NicSpanner::HandelTxrx: eth_span is null");
  assert(eth_span->IsComplete() and "eth span is not complete");

  tracer_.MarkSpanAsDone(eth_span);
  co_return true;
}

concurrencpp::lazy_result<bool> NicSpanner::HandelMsix(
    std::shared_ptr<concurrencpp::executor> resume_executor,
    std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  throw_if_empty(last_causing_, TraceException::kSpanIsNull, source_loc::current());
  auto msix_span = tracer_.StartSpanByParent<NicMsixSpan>(
      last_causing_, event_ptr, event_ptr->GetParserIdent(), name_);
  if (not msix_span) {
    std::cerr << "could not register msix span" << '\n';
    co_return false;
  }

  assert(msix_span->IsComplete() and "msix span is not complete");
  tracer_.MarkSpanAsDone(msix_span);

  spdlog::info("{} nic try push msix", name_);
  auto context = Context::CreatePassOnContext<expectation::kMsix>(msix_span);
  throw_on(not co_await to_host_queue_->Push(resume_executor, context),
           TraceException::kCouldNotPushToContextQueue,
           source_loc::current());
  spdlog::info("{} nic pushed msix", name_);

  co_return true;
}

NicSpanner::NicSpanner(
    TraceEnvironment &trace_environment,
    std::string &&name,
    Tracer &tra,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_network,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_network,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_host,
    std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_receives)
    : Spanner(trace_environment, std::move(name), tra),
      to_network_queue_(std::move(to_network)),
      from_network_queue_(std::move(from_network)),
      to_host_queue_(std::move(to_host)),
      from_host_queue_(std::move(from_host)),
      to_host_receives_(std::move(to_host_receives)) {
  throw_if_empty(to_network_queue_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(from_network_queue_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(to_host_queue_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(from_host_queue_, TraceException::kQueueIsNull, source_loc::current());
  throw_if_empty(to_host_receives_, TraceException::kQueueIsNull, source_loc::current());

  auto handel_mmio = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelMmio(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kNicMmioWT, handel_mmio);
  RegisterHandler(EventType::kNicMmioRT, handel_mmio);

  auto handel_dma = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelDma(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kNicDmaIT, handel_dma);
  RegisterHandler(EventType::kNicDmaExT, handel_dma);
  RegisterHandler(EventType::kNicDmaCWT, handel_dma);
  RegisterHandler(EventType::kNicDmaCRT, handel_dma);

  auto handel_tx_rx = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelTxrx(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kNicTxT, handel_tx_rx);
  RegisterHandler(EventType::kNicRxT, handel_tx_rx);

  auto handel_msix = [this](ExecutorT resume_executor, EventT &event_ptr) {
    return HandelMsix(std::move(resume_executor), event_ptr);
  };
  RegisterHandler(EventType::kNicMsixT, handel_msix);
}