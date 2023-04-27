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

#include <list>
#include <memory>

#include "corobelt.h"
#include "events.h"
#include "pack.h"
#include "queue.h"
#include "traceEnvironment.h"
#include "tracer.h"

#ifndef SIMBRICKS_TRACE_PACKER_H_
#define SIMBRICKS_TRACE_PACKER_H_

struct event_pack;
using pack_t = std::shared_ptr<event_pack>;
using event_t = std::shared_ptr<Event>;
using src_task = sim::corobelt::yield_task<event_t>;
using tar_task = sim::corobelt::yield_task<pack_t>;

struct packer : public sim::corobelt::transformer<event_t, pack_t> {
  uint64_t id_;

  // Override this method!
  virtual tar_task produce() override = 0;

  packer(sim::corobelt::producer<event_t> &prod)
      : sim::corobelt::transformer<event_t, pack_t>(prod),
        id_(trace_environment::get_next_packer_id()) {
  }

  bool ends_with_offset(uint64_t addr, uint64_t off) {
    size_t lz = std::__countl_zero(off);
    uint64_t mask = lz == 64 ? 0xffffffffffffffff : (1 << (64 - lz)) - 1;
    uint64_t check = addr & mask;
    return check == off;
  }

  template <class pt, class... Args>
  bool obtain_pack_ptr(std::shared_ptr<pt> &tar, Args &&...args) {
    if (tar) {
      return true;
    }

    tar = std::make_shared<pt>(args...);
    if (not tar) {
      return false;
    }

    return true;
  }

  template <class pt>
  std::shared_ptr<pt> iterate_add_erase(std::list<std::shared_ptr<pt>> &pending,
                                        std::shared_ptr<Event> event_ptr) {
    std::shared_ptr<pt> pending_pack = nullptr;

    for (auto it = pending.begin(); it != pending.end(); it++) {
      pending_pack = *it;
      if (pending_pack and pending_pack->add_to_pack(event_ptr)) {
        if (pending_pack->is_complete()) {
          pending.erase(it);
        }
        return pending_pack;
      }
    }

    return nullptr;
  }
};

struct host_packer : public packer {
  tar_task produce() override;

  host_packer(sim::corobelt::producer<event_t> &prod, context_queue &queue)
      : packer(prod), queue_(queue) {
  }

 private:
  context_queue &queue_;

  std::shared_ptr<host_call_pack> pending_host_call_pack_ = nullptr;
  std::shared_ptr<host_int_pack> pending_host_int_pack_ = nullptr;
  std::list<std::shared_ptr<host_dma_pack>> pending_host_dma_packs_;
  std::shared_ptr<host_mmio_pack> pending_host_mmio_pack_ = nullptr;
};

struct nic_packer : public packer {
  tar_task produce();

  nic_packer(sim::corobelt::producer<event_t> &prod, context_queue &host_queue,
             context_queue &network_queue)
      : packer(prod), host_queue_(host_queue), network_queue_(network_queue) {
  }

 private:
  context_queue &host_queue_;
  context_queue &network_queue_;

  std::list<std::shared_ptr<nic_dma_pack>> pending_nic_dma_packs_;
};

#endif  // SIMBRICKS_TRACE_PACKER_H_

/*

// TODO call pack


// TODO: dma packs --> multiple for host and nic
bool add_on_match(event_t event_ptr) override {
  if (not event_ptr) {
    return false;
  }

  switch (event_ptr->getType()) {
    case EventType::NicDmaI_t: {
      if (dma_issue_) {
        return false;
      }
      dma_issue_ = event_ptr;
      break;
    }

    case EventType::NicDmaEx_t: {
      if (not dma_issue_) {
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

    case EventType::HostDmaW_t:
    case EventType::HostDmaR_t: {
      if (not dma_issue_ or not nic_dma_execution_) {
        return false;
      }
      auto n_issue = std::static_pointer_cast<NicDmaI>(dma_issue_);

      is_read_ = is_type(event_ptr, EventType::HostDmaR_t);
      auto h_exec = std::static_pointer_cast<HostAddrSizeOp>(event_ptr);
      if (n_issue->addr_ != h_exec->addr_) {
        return false;
      }
      host_dma_execution_ = event_ptr;
      break;
    }

    case EventType::HostDmaC_t: {
      if (not dma_issue_ or not nic_dma_execution_ or
          not host_dma_execution_) {
        return false;
      }

      auto exec = std::static_pointer_cast<HostAddrSizeOp>(host_dma_execution_);
      auto comp = std::static_pointer_cast<HostDmaC>(event_ptr);
      if (exec->id_ != comp->id_) {
        return false;
      }

      host_dma_completion_ = event_ptr;
      break;
    }

    case EventType::NicDmaCW_t:
    case EventType::NicDmaCR_t: {
      if (not dma_issue_ or not nic_dma_execution_ or
          not host_dma_execution_ or not host_dma_completion_) {
        return false;
      }

      auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
      auto comp = std::static_pointer_cast<NicDma>(event_ptr);
      if (issue->id_ != comp->id_ or issue->addr_ != comp->addr_) {
        return false;
      }

      nic_dma_completion_ = event_ptr;
      is_pending_ = false;
      break;
    }

    default:
      return false;
  }

  add_to_pack(event_ptr);
  return true;
}

bool add_if_triggered(pack_t pack_ptr) override {
  if (not potentially_triggered(pack_ptr)) {
    return false;
  }

  if (not tx_rx_) {
    return false;
  }

  if (is_send_ and is_type(pack_ptr, pack_type::ETH_PACK)) {
    std::shared_ptr<eth_pack> np = std::static_pointer_cast<eth_pack>(pack_ptr);
    if (not np->tx_rx_ or np->is_send_) {
      return false;
    }

    add_triggered(pack_ptr);
    return true;

    // NOTE:
    //  - after Tx we expect a dma write to indicate sending
    //  - after Rx we expect data writes and register info that something was
    //  sent
  } else if (is_type(pack_ptr, pack_type::DMA_PACK)) {
    add_triggered(pack_ptr);
    return true;
  }

  return false;
}

// eth pack
bool add_on_match(event_t event_ptr) override {
  if (not event_ptr) {
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
  add_to_pack(event_ptr);
  return true;
}

// host interrrupt pack
bool add_on_match(event_t event_ptr) override {
  if (not event_ptr) {
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

  add_to_pack(event_ptr);
  return true;
}

// mmio pack
bool add_on_match(event_t event_ptr) override {
  if (not event_ptr) {
    return false;
  }

  switch (event_ptr->getType()) {
    case EventType::HostMmioW_t: {
      if (host_mmio_issue_) {
        return false;
      }
      is_read_ = false;
      host_mmio_issue_ = event_ptr;
      break;
    }
    case EventType::HostMmioR_t: {
      if (host_mmio_issue_ and not pci_msix_desc_addr_before_) {
        return false;
      }

      if (pci_msix_desc_addr_before_) {
        if (is_read_ or not host_mmio_issue_ or not im_mmio_resp_) {
          return false;
        }

        auto issue = std::static_pointer_cast<HostAddrSizeOp>(host_mmio_issue_);
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

    case EventType::NicMmioW_t:
    case EventType::NicMmioR_t: {
      if (is_type(event_ptr, EventType::NicMmioW_t)) {
        if (is_read_ or not host_mmio_issue_ or not im_mmio_resp_) {
          return false;
        }
      } else {
        if (not is_read_ or not host_mmio_issue_) {
          return false;
        }
      }

      if (pci_msix_desc_addr_before_) {
        return false;
      }

      auto issue = std::static_pointer_cast<HostAddrSizeOp>(host_mmio_issue_);
      auto nic_action = std::static_pointer_cast<NicMmio>(event_ptr);
      if (not ends_with_offset(issue->addr_, nic_action->off_)) {
        return false;
      }
      action_ = event_ptr;
      break;
    }

    case EventType::HostMmioCW_t:
    case EventType::HostMmioCR_t: {
      if (is_type(event_ptr, EventType::HostMmioCW_t)) {
        if (is_read_ or not host_mmio_issue_ or not im_mmio_resp_ or
            not action_) {
          return false;
        }
      } else {
        if (not is_read_ or not host_mmio_issue_ or not action_) {
          return false;
        }
      }

      if (pci_msix_desc_addr_before_) {
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

    default:
      return false;
  }

  add_to_pack(event_ptr);
  return true;
}

bool add_if_triggered(pack_t pack_ptr) override {
  if (not potentially_triggered(pack_ptr)) {
    return false;
  }

  if (is_read_ or not host_mmio_issue_) {
    return false;
  }

  if (pack_ptr->type_ != pack_type::DMA_PACK and
      pack_ptr->type_ != pack_type::ETH_PACK) {
    return false;
  }

  if (pack_ptr->type_ == pack_type::ETH_PACK) {
    auto p = std::static_pointer_cast<eth_pack>(pack_ptr);
    if (not p->tx_rx_ or not p->is_send_) {
      return false;
    }
  }

  add_triggered(pack_ptr);
  return true;
}

// msix
bool add_if_triggered(pack_t pack_ptr) override {
  return false;
}

bool add_on_match(event_t event_ptr) override {
  if (not event_ptr) {
    return false;
  }

  if (is_type(event_ptr, EventType::NicMsix_t)) {
    if (nic_msix_) {
      return false;
    }
    nic_msix_ = event_ptr;

  } else if (is_type(event_ptr, EventType::HostMsiX_t)) {
    if (not nic_msix_ or host_msix_) {
      return false;
    }
    if (nic_msix_->timestamp_ > event_ptr->timestamp_) {
      return false;
    }
    host_msix_ = event_ptr;
    is_pending_ = false;

  } else {
    return false;
  }

  add_to_pack(event_ptr);
  return true;
}

*/
