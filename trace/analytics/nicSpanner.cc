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

#include "spanner.h"

bool nic_spanner::handel_mmio(std::shared_ptr<Event> event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto con = host_queue_.poll(this->id_);
  if (not is_expectation(con, expectation::mmio)) {
    std::cerr << "nic_spanner: could not poll mmio context" << std::endl;
    return false;
  }
  last_host_context_ = con;

  auto mmio_span = tracer_.rergister_new_span_by_parent<nic_mmio_span>(
      last_host_context_->get_parent(), event_ptr->get_parser_ident());

  if (not mmio_span) {
    std::cerr << "could not register mmio_span" << std::endl;
    return false;
  }

  if (mmio_span->add_to_span(event_ptr)) {
    assert(mmio_span->is_complete() and "mmio span is not complete");
    last_completed_ = mmio_span;
    return true;
  }

  return false;
}

bool nic_spanner::handel_dma(std::shared_ptr<Event> event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto pending_dma =
      iterate_add_erase<nic_dma_span>(pending_nic_dma_spans_, event_ptr);
  if (pending_dma) {
    if (pending_dma->is_complete()) {
      last_completed_ = pending_dma;
    } else if (is_type(event_ptr, EventType::NicDmaEx_t)) {
      // indicate to host that we expect a dma action
      host_queue_.push(this->id_, expectation::dma, pending_dma);
    }

    return true;
  }

  assert(is_type(event_ptr, EventType::NicDmaI_t) and
         "try starting a new dma span with NON issue");

  pending_dma = tracer_.rergister_new_span_by_parent<nic_dma_span>(
      last_completed_, event_ptr->get_parser_ident());
  if (not pending_dma) {
    std::cerr << "could not register new pending dma action" << std::endl;
    return false;
  }

  if (pending_dma->add_to_span(event_ptr)) {
    pending_nic_dma_spans_.push_back(pending_dma);
    return true;
  }

  return false;
}
bool nic_spanner::handel_txrx(std::shared_ptr<Event> event_ptr) {
  assert(event_ptr and "event_ptr is null");

  bool is_tx = false;
  std::shared_ptr<event_span> parent = nullptr;
  if (is_type(event_ptr, EventType::NicTx_t)) {
    parent = last_completed_;
    is_tx = true;

  } else if (is_type(event_ptr, EventType::NicRx_t)) {
    auto con = network_queue_.poll(this->id_);
    if (is_expectation(con, expectation::rx)) {
      std::cerr << "nic_spanner: try to create receive span, but no receive ";
      std::cerr << "expectation from network" << std::endl;
      return false;
    }
    parent = con->get_parent();

  } else {
    return false;
  }
  
  auto eth_span = tracer_.rergister_new_span_by_parent<nic_eth_span>(
      parent, event_ptr->get_parser_ident());
  if (not eth_span) {
    std::cerr << "could not register eth_span" << std::endl;
    return false;
  }

  if (eth_span->add_to_span(event_ptr)) {
    assert(eth_span->is_complete() and "eth span was not complette");
    // indicate that somewhere a receive will be expected
    if (is_tx and
        not network_queue_.push(this->id_, expectation::rx, eth_span)) {
      std::cerr << "could not indicate to network that a";
      std::cerr << "receive is to be expected" << std::endl;
    }
    last_completed_ = eth_span;
    return true;
  }

  return false;
}

bool nic_spanner::handel_msix(std::shared_ptr<Event> event_ptr) {
  assert(event_ptr and "event_ptr is null");

  auto msix_span = tracer_.rergister_new_span_by_parent<nic_msix_span>(
      last_completed_, event_ptr->get_parser_ident());
  if (not msix_span) {
    std::cerr << "could not register msix span" << std::endl;
    return false;
  }

  if (msix_span->add_to_span(event_ptr)) {
    assert(msix_span->is_complete() and "msix span is not complete");
    host_queue_.push(this->id_, expectation::msix, last_completed_);
    return true;
  }

  return false;
}

sim::corobelt::task<void> nic_spanner::consume(
    sim::corobelt::yield_task<std::shared_ptr<Event>> *producer_task) {
  if (not producer_task) {
    co_return;
  }

  if (not host_queue_.register_spanner(id_) or
      not network_queue_.register_spanner(id_)) {
    std::cerr << "nic_packer " << id_;
    std::cerr << " error registering for host or network queue" << std::endl;
    co_return;
  }

  std::shared_ptr<Event> event_ptr = nullptr;
  std::shared_ptr<nic_dma_span> pending_dma = nullptr;
  bool added = false;

  while (producer_task and *producer_task) {
    event_ptr = producer_task->get();
    added = false;

    if (not event_ptr) {
      std::cerr << "found 'null' event" << std::endl;
      continue;
    }

    switch (event_ptr->get_type()) {
      case EventType::NicMmioW_t:
      case EventType::NicMmioR_t: {
        added = handel_mmio(event_ptr);
        break;
      }

      case EventType::NicDmaI_t:
      case EventType::NicDmaEx_t:
      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t: {
        added = handel_dma(event_ptr);
        break;
      }

      case EventType::NicTx_t:
      case EventType::NicRx_t: {
        added = handel_dma(event_ptr);
        break;
      }

      case EventType::NicMsix_t: {
        added = handel_msix(event_ptr);
        break;
      }

      default: {
        std::cout << "encountered non expected event ";
        if (event_ptr) {
          std::cout << *event_ptr << std::endl;
        } else {
          std::cout << "null" << std::endl;
        }
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
