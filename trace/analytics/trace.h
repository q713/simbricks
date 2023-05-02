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

#ifndef SIMBRICKS_TRACE_EVENT_TRACE_H_
#define SIMBRICKS_TRACE_EVENT_TRACE_H_

#include <iostream>
#include <memory>
#include <vector>

#include "span.h"
#include "corobelt.h"
#include "traceEnvironment.h"

struct trace {
  std::mutex mutex_;

  uint64_t id_;
  std::shared_ptr<event_span> parent_span_;

  // TODO: maybe store spans by source id...
  std::vector<std::shared_ptr<event_span>> spans_;

  bool is_done_ = false;

  inline bool is_done() {
    return is_done_;
  }

  inline void mark_as_done() {
    is_done_ = true;
  }

  bool add_span(std::shared_ptr<event_span> span) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (not span) {
      return false;
    }

    span->set_trace_id(id_);
    spans_.push_back(span);
  }

  void display(std::ostream &out) {
    std::lock_guard<std::mutex> lock(mutex_);
    out << std::endl;
    out << "trace: id=" << id_ << std::endl;
    for (auto span : spans_) {
      if (span) {
        if (span.get() == parent_span_.get()) {
          out << "\t parent_span:" << std::endl; 
        }
        span->display(out, 1);
      }
    }
    out << std::endl;
  }

  static std::shared_ptr<trace> create_trace(
      uint64_t id, std::shared_ptr<event_span> parent_span) {

    if (not parent_span) {
      return {};
    }

    auto t = std::shared_ptr<trace>{new trace{id, parent_span}};
    return t;
  }

 private:
  trace(uint64_t id, std::shared_ptr<event_span> parent_span)
      : id_(id), parent_span_(parent_span) {
    this->add_span(parent_span);
  }
};

struct trace_printer : public sim::corobelt::consumer<std::shared_ptr<trace>> {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<std::shared_ptr<trace>> *producer_task) {
    if (not producer_task) {
      co_return;
    }

    std::shared_ptr<trace> t;
    while (*producer_task) {
      t = producer_task->get();
      t->display(std::cout);
    }

    co_return;
  };

  trace_printer() : sim::corobelt::consumer<std::shared_ptr<trace>>() {
  }
};

/*
#include <bit>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/analytics/spans/callspan.h"
#include "trace/analytics/spans/dmaspan.h"
#include "trace/analytics/spans/ethspan.h"
#include "trace/analytics/spans/genericSinglespan.h"
#include "trace/analytics/spans/hostIntspan.h"
#include "trace/analytics/spans/mmiospan.h"
#include "trace/analytics/spans/msixspan.h"
#include "trace/analytics/spans/span.h"
#include "trace/corobelt/corobelt.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/events.h"
*/

// NOTE: currently analyzing a whole topology is not supported. only the
// analysis of a
//       nic/host pair is supported at the moment
// --> when extending: make sure that events in a span belong to same source!!!

/*
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
TODO: INCOOPERATE NEW GEM 5 INFOS (BAR OFFSET) INTO THIS ANALYSIS!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

/*
enum Stacks { KERNEL, NIC, SWITCH, NETWORK };

std::ostream &operator<<(std::ostream &out, Stacks s) {
  switch (s) {
    case KERNEL:
      out << "KERNEL";
      break;
    case NIC:
      out << "NIC";
      break;
    case SWITCH:
      out << "SWITCH";
      break;
    case NETWORK:
      out << "NETWORK";
      break;
    default:
      out << "UNDEFINED";
      break;
  }
  return out;
}

struct tcp_trace {
  using event_t = std::shared_ptr<Event>;
  using msg_t = std::optional<event_t>;
  using span_t = std::shared_ptr<event_span>;
  using callp_t = std::shared_ptr<call_span>;
  using mmiop_t = std::shared_ptr<mmio_span>;
  using dmap_t = std::shared_ptr<dma_span>;
  using msix_t = std::shared_ptr<msix_span>;
  using hostint_t = std::shared_ptr<host_int_span>;
  using trace_t = std::shared_ptr<tcp_trace>;

  sim::trace::env::trace_environment &env_;

  std::list<span_t> finished_spans_;  // TODO: print a trace in order

  std::list<callp_t> pending_call_spans_;
  std::list<mmiop_t> pending_mmio_spans_;
  std::list<dmap_t> pending_dma_spans_;
  std::list<msix_t> pending_msix_spans_;
  std::list<hostint_t> pending_hostint_spans_;

  bool is_tcp_handshake_ = false;
  bool is_tcp_tx_rx_ = false;
  bool last_call_pci_msix_desc_addr_ = false;
  size_t expected_tx_ = 0;
  size_t expected_rx_ = 0;
  size_t driver_tx_ = 0;
  size_t driver_rx_ = 0;
  size_t nic_tx_ = 0;
  size_t nic_rx_ = 0;

  // state to decide what dam operations belong to
  span_t last_finished_dma_causing_span_ = nullptr;

  tcp_trace(sim::trace::env::trace_environment &env) : env_(env){};

  ~tcp_trace() = default;

  bool is_trace_pending() {
    return not has_finished_spans() or has_pending_call() or
           has_pending_mmio() or has_pending_msix() or has_pending_dma() or
           has_expected_transmits_or_receives();
    // not found_host_tx_ or not found_host_rx_;
    // return not has_finished_spans() or has_pending_call() or
    // has_pending_mmio() or
    //        has_pending_dma() or not found_host_tx_ or not found_host_rx_ or
    //        not found_nic_tx_ or not found_nic_rx_;
  }

  inline bool is_trace_done() {
    return not is_trace_pending();
  }

  inline bool has_expected_transmits() {
    return expected_tx_ < driver_tx_ and expected_tx_ < nic_tx_;
  }

  inline bool has_expected_receives() {
    return expected_rx_ < driver_rx_ and expected_rx_ < nic_rx_;
  }

  inline bool has_expected_transmits_or_receives() {
    return has_expected_transmits() or has_expected_receives();
  }

  inline bool has_finished_spans() {
    return not finished_spans_.empty();
  }

  inline bool has_pending_call() {
    return not pending_call_spans_.empty();
  }

  bool is_new_call_needed() {
    return expected_tx_ <= 0 or expected_rx_ <= 0 or
           has_expected_transmits_or_receives();
  }

  inline bool has_pending_mmio() {
    return not pending_mmio_spans_.empty();
  }

  bool is_new_mmio_needed() {
    return has_expected_transmits() or has_pending_call();
  }

  inline bool has_pending_dma() {
    return not pending_dma_spans_.empty();
  }

  bool is_new_dma_needed() {
    return has_expected_transmits_or_receives() or has_pending_call();
  }

  inline bool has_pending_msix() {
    return not pending_msix_spans_.empty();
  }

  inline bool has_pending_hostint() {
    return not pending_hostint_spans_.empty();
  }

  bool is_tcp_handshake() {
    return is_tcp_handshake_;
  }

  bool is_tcp_tx_rx() {
    return is_tcp_tx_rx_;
  }

  inline bool is_handshake_or_tx_rx() {
    return is_tcp_handshake() or is_tcp_tx_rx();
  }

  bool add_to_trace(event_t event_ptr) {
    if (not event_ptr or not is_trace_pending()) {
      return false;
    }

    bool added = false;
    switch (event_ptr->getType()) {
      case EventType::HostCall_t:
        added = add_call(event_ptr);
        break;

      case EventType::HostMmioW_t:
      case EventType::HostMmioR_t:
      case EventType::HostMmioImRespPoW_t:
      case EventType::NicMmioW_t:
      case EventType::NicMmioR_t:
      case EventType::HostMmioCW_t:
      case EventType::HostMmioCR_t:
        added = add_mmio(event_ptr);
        break;

      case EventType::NicDmaI_t:
      case EventType::NicDmaEx_t:
      case EventType::HostDmaW_t:
      case EventType::HostDmaR_t:
      case EventType::HostDmaC_t:
      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t:
        added = add_dma(event_ptr);
        break;

      case EventType::NicTx_t:
      case EventType::NicRx_t:
        added = add_eth(event_ptr);
        break;

      case EventType::NicMsix_t:
      case EventType::HostMsiX_t:
        added = add_msix(event_ptr);
        break;

      case EventType::HostPostInt_t:
      case EventType::HostClearInt_t:
        added = add_host_int(event_ptr);
        break;

      default:
        added = add_generic_single(event_ptr);
        break;
    }

    return added;
  }

  void display(std::ostream &out) {
    out << std::endl;
    out << std::endl;
    out << "Event Trace:" << std::endl;
    out << "\t expected transmits: " << expected_tx_ << std::endl;
    out << "\t expected receives: " << expected_rx_ << std::endl;
    out << "\t driver transmits: " << driver_tx_ << std::endl;
    out << "\t driver receives: " << driver_rx_ << std::endl;
    out << "\t nic transmits: " << nic_tx_ << std::endl;
    out << "\t nic receives: " << nic_rx_ << std::endl;
    out << "\tFinished spans:" << std::endl;
    for (auto span : finished_spans_) {
      span->display(out, 2);
      out << std::endl;
    }
    out << std::endl;
    out << "\tPendingspans:" << std::endl;
    if (not(has_pending_call() or has_pending_dma() or has_pending_mmio())) {
      out << "\t\tNone" << std::endl;
      out << std::endl;
      out << std::endl;
      return;
    }

    for (auto span : pending_call_spans_) {
      span->display(out, 2);
      out << std::endl;
    }
    for (auto span : pending_mmio_spans_) {
      span->display(out, 2);
      out << std::endl;
    }
    for (auto span : pending_dma_spans_) {
      span->display(out, 2);
      out << std::endl;
    }
    out << std::endl;
    out << std::endl;
  }

 private:
  void add_span(span_t span) {
    if (not span or span->is_pending()) {
      return;
    }

    if (span->type_ == span_type::CALL_span) {
      auto cp = std::static_pointer_cast<call_span>(span);
      // in case a call span is non networking related we filter it out
      if (not cp->is_relevant_) {
        //std::cout << "filtered out non relevant call span:" << std::endl;
        // cp->display(std::cout);
        // std::cout << std::endl;
        return;
      }
    }

    finished_spans_.push_back(span);
    // std::cout << "add a span: " << std::endl;
    // span->display(std::cout, 1);
  }

  template <typename span_type>
  std::shared_ptr<span_type> iterate_add_erase(
      std::list<std::shared_ptr<span_type>> &pending, event_t event_ptr) {
    auto it = pending.begin();
    while (it != pending.end()) {
      auto cur_span = *it;
      if (cur_span->is_complete()) {
        add_span(cur_span);
        it = pending.erase(it);
      } else {
        if (cur_span->add_on_match(event_ptr)) {
          if (cur_span->is_complete()) {
            add_span(cur_span);
            pending.erase(it);
          }
          return cur_span;
        }
        it++;
      }
    }
    return nullptr;
  }

  template <typename pt>
  std::shared_ptr<pt> create_add(std::list<std::shared_ptr<pt>> &pending,
                                 event_t event_ptr) {
    auto pending_stack = std::make_shared<pt>(env_);
    if (pending_stack and pending_stack->add_on_match(event_ptr)) {
      pending.push_back(pending_stack);
      return pending_stack;
    }
    return nullptr;
  }

  template <typename pt>
  bool add_set_triggered_pending(std::list<std::shared_ptr<pt>> &pending,
                                 span_t span_ptr) {
    if (not span_ptr or span_ptr->is_pending()) {
      return false;
    }

    for (std::shared_ptr<pt> span : pending) {
      if (span->add_if_triggered(span_ptr)) {
        span_ptr->set_triggered_by(span);
        return true;
      }
    }

    return false;
  }

  template <typename pt>
  bool add_set_triggered_trace(span_type type, span_t span_ptr) {
    if (not span_ptr or span_ptr->is_pending()) {
      return false;
    }

    for (auto it = finished_spans_.rbegin(); it != finished_spans_.rend();
         it++) {
      span_t span = *it;
      if (span->type_ != type) {
        continue;
      }

      auto casted_span = std::static_pointer_cast<pt>(span);
      if (casted_span->add_if_triggered(span_ptr)) {
        span_ptr->set_triggered_by(casted_span);
        return true;
      }
    }

    return false;
  }

  bool add_call(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<call_span> span = nullptr;
    if (has_pending_call()) {
      span = iterate_add_erase<call_span>(pending_call_spans_, event_ptr);
    }

    if (not span and is_new_call_needed()) {
      span = create_add<call_span>(pending_call_spans_, event_ptr);
    }

    if (span) {
      // remove mmio spans after pci_msix_desc_addr
      if (not env_.is_pci_msix_desc_addr(event_ptr)) {
        if (last_call_pci_msix_desc_addr_) {
          auto it = pending_mmio_spans_.begin();
          while (it != pending_mmio_spans_.end()) {
            mmiop_t span = *it;
            if (not span->is_pending()) {
              add_span(span);
              it = pending_mmio_spans_.erase(it);
            } else if (span->is_pending() and
                       span->pci_msix_desc_addr_before_ and
                       span->host_mmio_issue_ and span->im_mmio_resp_) {
              add_span(span);
              it = pending_mmio_spans_.erase(it);
            } else {
              ++it;
            }
          }
        }
        last_call_pci_msix_desc_addr_ = false;
      } else {
        last_call_pci_msix_desc_addr_ = true;
      }

      if (env_.is_socket_connect(event_ptr)) {
        expected_tx_ += 3;
        expected_rx_ += 2;
        span->mark_as_relevant();
      } else if (env_.is_nw_interface_send(event_ptr)) {
        ++expected_tx_;
        span->mark_as_relevant();
      } else if (env_.is_nw_interface_receive(event_ptr)) {
        ++expected_rx_;
        span->mark_as_relevant();
      } else if (env_.is_driver_tx(event_ptr)) {
        ++driver_tx_;
        span->mark_as_relevant();
      } else if (env_.is_driver_rx(event_ptr)) {
        // TODO
        ++driver_rx_;
        span->mark_as_relevant();
      }

      return true;
    }

    return false;
  }

  bool add_mmio(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<mmio_span> span = nullptr;
    if (has_pending_mmio()) {
      span = iterate_add_erase<mmio_span>(pending_mmio_spans_, event_ptr);

      if (span and span->is_complete() and span->is_write()) {
        // TODO: only if is not pending anymore
        add_set_triggered_pending<call_span>(pending_call_spans_, span);
        last_finished_dma_causing_span_ = span;
      }
    }

    // found everything of this trace, hence do not create a new event
    // as it must have been issued by another trace
    if (not is_new_mmio_needed()) {
      if (span) {
        return true;
      }
      return false;
    }

    if (not span) {
      span = std::make_shared<mmio_span>(last_call_pci_msix_desc_addr_, env_);
      if (not span or not span->add_on_match(event_ptr)) {
        return false;
      }
      pending_mmio_spans_.push_back(span);
    }

    return true;
  }

  bool add_dma(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<dma_span> span = nullptr;
    if (has_pending_dma()) {
      span = iterate_add_erase<dma_span>(pending_dma_spans_, event_ptr);

      if (span and span->is_complete() and last_finished_dma_causing_span_) {
        if (last_finished_dma_causing_span_->get_type() ==
            span_type::MMIO_span) {
          // TODO: after Mmio write, we expect Dma Reads
          add_set_triggered_trace<mmio_span>(span_type::MMIO_span, span);

        } else if (last_finished_dma_causing_span_->get_type() ==
                   span_type::ETH_span) {
          // TODO: after Rx/Tx, we expect Dma Writes/Write
          add_set_triggered_trace<eth_span>(span_type::ETH_span, span);
        }
      }
    }

    // found everything of this trace, hence do not create a new event
    // as it must have been issued by another trace
    if (not is_new_dma_needed()) {
      if (span) {
        return true;
      }
      return false;
    }

    if (not span) {
      span = create_add<dma_span>(pending_dma_spans_, event_ptr);
      if (nullptr == span) {
        return false;
      }
    }

    return true;
  }

  bool add_eth(event_t event_ptr) {
    if (not event_ptr or
        (not has_expected_transmits_or_receives() and not has_pending_call())) {
      return false;
    }

    auto p = std::make_shared<eth_span>(env_);
    if (p and p->add_on_match(event_ptr) and p->is_complete()) {
      add_span(p);

      if (is_type(event_ptr, EventType::NicTx_t)) {
        nic_tx_++;
      } else if (is_type(event_ptr, EventType::NicRx_t)) {
        nic_rx_++;
      }

      if (p->is_transmit()) {
        add_set_triggered_trace<mmio_span>(span_type::MMIO_span, p);
      } else {
        add_set_triggered_trace<eth_span>(span_type::ETH_span, p);
      }

      last_finished_dma_causing_span_ = p;
      return true;
    }
    return false;
  }

  bool add_msix(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    msix_t span = nullptr;
    if (has_pending_msix()) {
      span = iterate_add_erase<msix_span>(pending_msix_spans_, event_ptr);
    }

    if (not span) {
      span = create_add<msix_span>(pending_msix_spans_, event_ptr);
      if (not span) {
        return false;
      }
    }

    return true;
  }

  bool add_host_int(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    hostint_t span = nullptr;
    if (has_pending_hostint()) {
      span =
          iterate_add_erase<host_int_span>(pending_hostint_spans_, event_ptr);
    }

    if (not span) {
      span = create_add<host_int_span>(pending_hostint_spans_, event_ptr);
      if (not span) {
        return false;
      }
    }

    return true;
  }

  bool add_generic_single(event_t event_ptr) {
    if (not event_ptr or not is_trace_pending()) {
      return false;
    }

    auto span = std::make_shared<single_event_span>(env_);
    if (span and span->add_on_match(event_ptr)) {
      add_span(span);

      // TODO: any triggers?!?!?!

      return true;
    }
    return false;
  }
};


struct trace_printer
    : public sim::corobelt::consumer<std::shared_ptr<tcp_trace>> {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<std::shared_ptr<tcp_trace>> *producer_task) {
    if (not producer_task) {
      co_return;
    }

    std::shared_ptr<tcp_trace> t;
    while (*producer_task) {
      t = producer_task->get();
      t->display(std::cout);
    }

    co_return;
  };

  trace_printer() : sim::corobelt::consumer<std::shared_ptr<tcp_trace>>() {
  }
};

*/

#endif  // SIMBRICKS_TRACE_EVENT_TRACE_H_