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

#include <bit>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/analytics/packs/callPack.h"
#include "trace/analytics/packs/dmaPack.h"
#include "trace/analytics/packs/ethPack.h"
#include "trace/analytics/packs/genericSinglePack.h"
#include "trace/analytics/packs/hostIntPack.h"
#include "trace/analytics/packs/mmioPack.h"
#include "trace/analytics/packs/msixPack.h"
#include "trace/analytics/packs/pack.h"
#include "trace/corobelt/corobelt.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/events.h"

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

// NOTE: currently analyzing a whole topology is not supported. only the
// analysis of a
//       nic/host pair is supported at the moment
// --> when extending: make sure that events in a pack belong to same source!!!
struct tcp_trace {
  using event_t = std::shared_ptr<Event>;
  using msg_t = std::optional<event_t>;
  using pack_t = std::shared_ptr<event_pack>;
  using callp_t = std::shared_ptr<call_pack>;
  using mmiop_t = std::shared_ptr<mmio_pack>;
  using dmap_t = std::shared_ptr<dma_pack>;
  using msix_t = std::shared_ptr<msix_pack>;
  using hostint_t = std::shared_ptr<host_int_pack>;
  using trace_t = std::shared_ptr<tcp_trace>;

  sim::trace::env::trace_environment &env_;

  std::list<pack_t> finished_packs_;  // TODO: print a trace in order

  std::list<callp_t> pending_call_packs_;
  std::list<mmiop_t> pending_mmio_packs_;
  std::list<dmap_t> pending_dma_packs_;
  std::list<msix_t> pending_msix_packs_;
  std::list<hostint_t> pending_hostint_packs_;

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
  pack_t last_finished_dma_causing_pack_ = nullptr;

  tcp_trace(sim::trace::env::trace_environment &env) : env_(env){};

  ~tcp_trace() = default;

  bool is_trace_pending() {
    return not has_finished_packs() or has_pending_call() or
           has_pending_mmio() or has_pending_msix() or has_pending_dma() or
           has_expected_transmits_or_receives();
    // not found_host_tx_ or not found_host_rx_;
    // return not has_finished_packs() or has_pending_call() or
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

  inline bool has_finished_packs() {
    return not finished_packs_.empty();
  }

  inline bool has_pending_call() {
    return not pending_call_packs_.empty();
  }

  bool is_new_call_needed() {
    return has_expected_transmits_or_receives();
  }

  inline bool has_pending_mmio() {
    return not pending_mmio_packs_.empty();
  }

  bool is_new_mmio_needed() {
    return has_expected_transmits() or has_pending_call();
  }

  inline bool has_pending_dma() {
    return not pending_dma_packs_.empty();
  }

  bool is_new_dma_needed() {
    return has_expected_transmits_or_receives() or has_pending_call();
  }

  inline bool has_pending_msix() {
    return not pending_msix_packs_.empty();
  }

  inline bool has_pending_hostint() {
    return not pending_hostint_packs_.empty();
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
    out << "\tFinished Packs:" << std::endl;
    for (auto pack : finished_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    out << std::endl;
    out << "\tPendingPacks:" << std::endl;
    if (not(has_pending_call() or has_pending_dma() or has_pending_mmio())) {
      out << "\t\tNone" << std::endl;
      out << std::endl;
      out << std::endl;
      return;
    }

    for (auto pack : pending_call_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    for (auto pack : pending_mmio_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    for (auto pack : pending_dma_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    out << std::endl;
    out << std::endl;
  }

 private:
  void add_pack(pack_t pack) {
    if (not pack or pack->is_pending()) {
      return;
    }

    if (pack->type_ == pack_type::CALL_PACK) {
      auto cp = std::static_pointer_cast<call_pack>(pack);
      // in case a call pack is non networking related we filter it out
      if (not cp->is_relevant_) {
        // std::cout << "filtered out non relevant call pack:" << std::endl;
        // cp->display(std::cout);
        // std::cout << std::endl;
        return;
      }
    }

    finished_packs_.push_back(pack);
    // std::cout << "add a pack: " << std::endl;
    // pack->display(std::cout, 1);
  }

  template <typename pack_type>
  std::shared_ptr<pack_type> iterate_add_erase(
      std::list<std::shared_ptr<pack_type>> &pending, event_t event_ptr) {
    auto it = pending.begin();
    while (it != pending.end()) {
      auto cur_pack = *it;
      if (cur_pack->is_complete()) {
        add_pack(cur_pack);
        it = pending.erase(it);
      } else {
        if (cur_pack->add_on_match(event_ptr)) {
          if (cur_pack->is_complete()) {
            add_pack(cur_pack);
            pending.erase(it);
          }
          return cur_pack;
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
                                 pack_t pack_ptr) {
    if (not pack_ptr or pack_ptr->is_pending()) {
      return false;
    }

    for (std::shared_ptr<pt> pack : pending) {
      if (pack->add_if_triggered(pack_ptr)) {
        pack_ptr->set_triggered_by(pack);
        return true;
      }
    }

    return false;
  }

  template <typename pt>
  bool add_set_triggered_trace(pack_type type, pack_t pack_ptr) {
    if (not pack_ptr or pack_ptr->is_pending()) {
      return false;
    }

    for (auto it = finished_packs_.rbegin(); it != finished_packs_.rend();
         it++) {
      pack_t pack = *it;
      if (pack->type_ != type) {
        continue;
      }

      auto casted_pack = std::static_pointer_cast<pt>(pack);
      if (casted_pack->add_if_triggered(pack_ptr)) {
        pack_ptr->set_triggered_by(casted_pack);
        return true;
      }
    }

    return false;
  }

  bool add_call(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<call_pack> pack = nullptr;
    if (has_pending_call()) {
      pack = iterate_add_erase<call_pack>(pending_call_packs_, event_ptr);
    }

    if (not pack and is_new_call_needed()) {
      pack = create_add<call_pack>(pending_call_packs_, event_ptr);
    }

    if (pack) {
      // remove mmio packs after pci_msix_desc_addr
      if (not env_.is_pci_msix_desc_addr(event_ptr)) {
        if (last_call_pci_msix_desc_addr_) {
          auto it = pending_mmio_packs_.begin();
          while (it != pending_mmio_packs_.end()) {
            mmiop_t pack = *it;
            if (not pack->is_pending()) {
              add_pack(pack);
              it = pending_mmio_packs_.erase(it);
            } else if (pack->is_pending() and
                       pack->pci_msix_desc_addr_before_ and
                       pack->host_mmio_issue_ and pack->im_mmio_resp_) {
              add_pack(pack);
              it = pending_mmio_packs_.erase(it);
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
      } else if (env_.is_nw_interface_send(event_ptr)) {
        ++expected_tx_;
      } else if (env_.is_nw_interface_receive(event_ptr)) {
        ++expected_rx_;
      } else if (env_.is_driver_tx(event_ptr)) {
        ++driver_tx_;
      } else if (env_.is_driver_rx(event_ptr)) {
        // TODO
        ++driver_rx_;
      }

      return true;
    }

    return false;
  }

  bool add_mmio(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<mmio_pack> pack = nullptr;
    if (has_pending_mmio()) {
      pack = iterate_add_erase<mmio_pack>(pending_mmio_packs_, event_ptr);

      if (pack and pack->is_complete() and pack->is_write()) {
        // TODO: only if is not pending anymore
        add_set_triggered_pending<call_pack>(pending_call_packs_, pack);
        last_finished_dma_causing_pack_ = pack;
      }
    }

    // found everything of this trace, hence do not create a new event
    // as it must have been issued by another trace
    if (not is_new_mmio_needed()) {
      if (pack) {
        return true;
      }
      return false;
    }

    if (not pack) {
      pack = std::make_shared<mmio_pack>(last_call_pci_msix_desc_addr_, env_);
      if (not pack or not pack->add_on_match(event_ptr)) {
        return false;
      }
      pending_mmio_packs_.push_back(pack);
    }

    return true;
  }

  bool add_dma(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<dma_pack> pack = nullptr;
    if (has_pending_dma()) {
      pack = iterate_add_erase<dma_pack>(pending_dma_packs_, event_ptr);

      if (pack and pack->is_complete() and last_finished_dma_causing_pack_) {
        if (last_finished_dma_causing_pack_->get_type() ==
            pack_type::MMIO_PACK) {
          // TODO: after Mmio write, we expect Dma Reads
          add_set_triggered_trace<mmio_pack>(pack_type::MMIO_PACK, pack);

        } else if (last_finished_dma_causing_pack_->get_type() ==
                   pack_type::ETH_PACK) {
          // TODO: after Rx/Tx, we expect Dma Writes/Write
          add_set_triggered_trace<eth_pack>(pack_type::ETH_PACK, pack);
        }
      }
    }

    // found everything of this trace, hence do not create a new event
    // as it must have been issued by another trace
    if (not is_new_dma_needed()) {
      if (pack) {
        return true;
      }
      return false;
    }

    if (not pack) {
      pack = create_add<dma_pack>(pending_dma_packs_, event_ptr);
      if (nullptr == pack) {
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

    auto p = std::make_shared<eth_pack>(env_);
    if (p and p->add_on_match(event_ptr) and p->is_complete()) {
      add_pack(p);

      if (is_type(event_ptr, EventType::NicTx_t)) {
        nic_tx_++;
      } else if (is_type(event_ptr, EventType::NicRx_t)) {
        nic_rx_++;
      }

      if (p->is_transmit()) {
        add_set_triggered_trace<mmio_pack>(pack_type::MMIO_PACK, p);
      } else {
        add_set_triggered_trace<eth_pack>(pack_type::ETH_PACK, p);
      }

      last_finished_dma_causing_pack_ = p;
      return true;
    }
    return false;
  }

  bool add_msix(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    msix_t pack = nullptr;
    if (has_pending_msix()) {
      pack = iterate_add_erase<msix_pack>(pending_msix_packs_, event_ptr);
    }

    if (not pack) {
      pack = create_add<msix_pack>(pending_msix_packs_, event_ptr);
      if (not pack) {
        return false;
      }
    }

    return true;
  }

  bool add_host_int(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    hostint_t pack = nullptr;
    if (has_pending_hostint()) {
      pack =
          iterate_add_erase<host_int_pack>(pending_hostint_packs_, event_ptr);
    }

    if (not pack) {
      pack = create_add<host_int_pack>(pending_hostint_packs_, event_ptr);
      if (not pack) {
        return false;
      }
    }

    return true;
  }

  bool add_generic_single(event_t event_ptr) {
    if (not event_ptr or not is_trace_pending()) {
      return false;
    }

    auto pack = std::make_shared<single_event_pack>(env_);
    if (pack and pack->add_on_match(event_ptr)) {
      add_pack(pack);

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

#endif  // SIMBRICKS_TRACE_EVENT_TRACE_H_