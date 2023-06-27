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
#include <memory>

#include "analytics/spanner.h"
#include "util/exception.h"

concurrencpp::lazy_result<bool>
NicSpanner::HandelMmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  std::cout << "nic try poll mmio" << std::endl;
  auto con_opt = co_await from_host_queue_->pop(resume_executor);
  throw_on(not con_opt.has_value(), context_is_null);
  auto con = con_opt.value();
  throw_if_empty(con, context_is_null);
  std::cout << "nic polled mmio" << std::endl;
  if (not is_expectation(con, expectation::kMmio)) {
    std::cerr << "nic_spanner: could not poll mmio context" << std::endl;
    co_return false;
  }
  last_host_context_ = con;

  auto mmio_span = tracer_.StartSpanByParent<NicMmioSpan>(
      name_, con->GetNonEmptyParent(), event_ptr, event_ptr->get_parser_ident());
  if (not mmio_span) {
    std::cerr << "could not register mmio_span" << std::endl;
    co_return false;
  }

  assert(mmio_span->IsComplete() and "mmio span is not complete");
  tracer_.MarkSpanAsDone(mmio_span);
  //last_completed_ = mmio_span;
  if (mmio_span->IsWrite()) {
    last_causing_ = mmio_span;
  }
  co_return true;
}

concurrencpp::lazy_result<bool>
NicSpanner::HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto pending_dma =
      iterate_add_erase<NicDmaSpan>(pending_nic_dma_spans_, event_ptr);
  if (pending_dma) {
    if (pending_dma->IsComplete()) {
      tracer_.MarkSpanAsDone(pending_dma);
      //last_completed_ = pending_dma;
    } else if (is_type(event_ptr, EventType::NicDmaEx_t)) {
      // indicate to host that we expect a dma action
      std::cout << "nic try push dma" << std::endl;
      auto context = create_shared<Context>(
          "HandelDma could not create context", expectation::kDma, pending_dma);
      throw_on(not co_await to_host_queue_->push(resume_executor, context),
               could_not_push_to_context_queue);
      std::cout << "nic pushed dma" << std::endl;
    }

    co_return true;
  }

  assert(is_type(event_ptr, EventType::NicDmaI_t) and
      "try starting a new dma span with NON issue");

  pending_dma = tracer_.StartSpanByParent<NicDmaSpan>(name_, last_causing_,
                                                      event_ptr, event_ptr->get_parser_ident());
  if (not pending_dma) {
    std::cerr << "could not register new pending dma action" << std::endl;
    co_return false;
  }

  pending_nic_dma_spans_.push_back(pending_dma);
  co_return true;
}
concurrencpp::lazy_result<bool>
NicSpanner::HandelTxrx(std::shared_ptr<concurrencpp::executor> resume_executor,
                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  bool is_tx = false;
  std::shared_ptr<EventSpan> parent = nullptr;
  if (is_type(event_ptr, EventType::NicTx_t)) {
    // parent = last_completed_;
    parent = last_causing_;
    is_tx = true;
    last_action_was_send_ = true;

  } else if (is_type(event_ptr, EventType::NicRx_t)) {
    //auto con = co_await network_queue_.poll(resume_executor, this->id_);
    //if (is_expectation(con, expectation::rx)) {
    //  std::cerr << "nic_spanner: try to create receive span, but no receive ";
    //  std::cerr << "expectation from network" << std::endl;
    //  co_return false;
    //}
    //parent = con->GetParent();
    //parent = last_completed_;
    parent = last_causing_; // TODO: change when incoperating server node!!!
    is_tx = false;
    last_action_was_send_ = false;

  } else {
    co_return false;
  }

  auto eth_span = tracer_.StartSpanByParent<NicEthSpan>(
      name_, parent, event_ptr, event_ptr->get_parser_ident());
  if (not eth_span) {
    std::cerr << "could not register eth_span" << std::endl;
    co_return false;
  }

  assert(eth_span->IsComplete() and "eth span was not complette");
  // indicate that somewhere a receive will be expected
  //if (is_tx and
  //    not co_await network_queue_.push(resume_executor, this->id_, expectation::rx, eth_span)) {
  //  std::cerr << "could not indicate to network that a";
  //  std::cerr << "receive is to be expected" << std::endl;
  //}
  tracer_.MarkSpanAsDone(eth_span);
  //last_completed_ = eth_span;
  // TODO: if it is a transmit send out to network queue
  if (not is_tx) {
    last_causing_ = eth_span;
  }
  co_return true;
}

concurrencpp::lazy_result<bool>
NicSpanner::HandelMsix(std::shared_ptr<concurrencpp::executor> resume_executor,
                       std::shared_ptr<Event> &event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto msix_span = tracer_.StartSpanByParent<NicMsixSpan>(
      name_, last_causing_, event_ptr, event_ptr->get_parser_ident());
  if (not msix_span) {
    std::cerr << "could not register msix span" << std::endl;
    co_return false;
  }

  assert(msix_span->IsComplete() and "msix span is not complete");
  tracer_.MarkSpanAsDone(msix_span);

  std::cout << "nic try push msix" << std::endl;
  auto context = create_shared<Context>(
      "HandelMsix could not create context", expectation::kMsix, msix_span);
  throw_on(not co_await to_host_queue_->push(resume_executor, context),
           could_not_push_to_context_queue);
  std::cout << "nic pushed msix" << std::endl;

  co_return true;
}

concurrencpp::result<void>
NicSpanner::consume(std::shared_ptr<concurrencpp::executor> resume_executor,
                    std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan) {
  throw_if_empty(resume_executor, resume_executor_null);
  throw_if_empty(src_chan, channel_is_null);

  std::shared_ptr<Event> event_ptr = nullptr;
  std::shared_ptr<NicDmaSpan> pending_dma = nullptr;
  bool added = false;
  std::optional<std::shared_ptr<Event>> event_ptr_opt;

  for (event_ptr_opt = co_await src_chan->pop(resume_executor); event_ptr_opt.has_value();
       event_ptr_opt = co_await src_chan->pop(resume_executor)) {

    event_ptr = event_ptr_opt.value();
    throw_if_empty(event_ptr, event_is_null);

    added = false;

    std::cout << "nic spanner try handel: " << *event_ptr << std::endl;

    switch (event_ptr->get_type()) {
      case EventType::NicMmioW_t:
      case EventType::NicMmioR_t: {
        added = co_await HandelMmio(resume_executor, event_ptr);
        break;
      }

      case EventType::NicDmaI_t:
      case EventType::NicDmaEx_t:
      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t: {
        added = co_await HandelDma(resume_executor, event_ptr);
        break;
      }

      case EventType::NicTx_t:
      case EventType::NicRx_t: {
        added = co_await HandelTxrx(resume_executor, event_ptr);
        break;
      }

      case EventType::NicMsix_t: {
        added = co_await HandelMsix(resume_executor, event_ptr);
        break;
      }

      default: {
        std::cout << "encountered non expected event ";
        std::cout << *event_ptr << std::endl;
        added = false;
        break;
      }
    }

    if (not added) {
      std::cout << "found event that could not be added to a pack: "
                << *event_ptr << std::endl;
    }
  }

  co_return;
}
