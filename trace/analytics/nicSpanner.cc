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

#include "spanner.h"

using pack_t = std::shared_ptr<event_pack>;
using event_t = std::shared_ptr<Event>;
using src_task = sim::corobelt::yield_task<event_t>;
using tar_task = sim::corobelt::task<void>;

sim::corobelt::task<void> nic_spanner::consume(
    sim::corobelt::yield_task<event_t> *producer_task) {
  if (not producer_task) {
    co_return;
  }

  if (not host_queue_.register_spanner(id_) or
      not network_queue_.register_spanner(id_)) {
    std::cerr << "nic_packer " << id_;
    std::cerr << " error registering for host or network queue" << std::endl;
    co_return;
  }

  event_t event_ptr = nullptr;
  std::shared_ptr<nic_dma_pack> pending_dma = nullptr;
  bool added = false;

  while (producer_task and *producer_task) {
    event_ptr = producer_task->get();
    added = false;

    switch (event_ptr->getType()) {
      case EventType::NicMmioW_t:
      case EventType::NicMmioR_t: {
        std::shared_ptr<nic_mmio_pack> mmio_p = nullptr;
        if (not obtain_pack_ptr<nic_mmio_pack>(mmio_p)) {
          std::cerr << "could not allocate mmio_p" << std::endl;
          break;
        }

        if (mmio_p->add_to_pack(event_ptr)) {
          added = true;
          if (mmio_p->is_complete()) {
            co_yield mmio_p;  // TODO: remove
            mmio_p = nullptr;
          }
        }
        break;
      }

      case EventType::NicDmaI_t:
      case EventType::NicDmaEx_t:
      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t: {
        pending_dma =
            iterate_add_erase<nic_dma_pack>(pending_nic_dma_packs_, event_ptr);
        if (pending_dma) {
          added = true;
          if (pending_dma->is_complete()) {
            co_yield pending_dma;  // TODO: remove
          }
          break;
        }

        pending_dma = nullptr;
        if (not obtain_pack_ptr<nic_dma_pack>(pending_dma)) {
          std::cerr << "could not allocate pending_nic_dma_packs_" << std::endl;
          break;
        }

        if (pending_dma->add_to_pack(event_ptr)) {
          added = true;
          pending_nic_dma_packs_.push_back(pending_dma);
        }
        break;
      }

      case EventType::NicTx_t:
      case EventType::NicRx_t: {
        std::shared_ptr<nic_eth_pack> eth_p = nullptr;
        if (not obtain_pack_ptr<nic_eth_pack>(eth_p)) {
          std::cerr << "could not allocate eth_p" << std::endl;
          break;
        }

        if (eth_p->add_to_pack(event_ptr)) {
          added = true;
          if (eth_p->is_complete()) {
            co_yield eth_p;  // TODO: remove
            eth_p = nullptr;
          }
        }
        break;
      }

      case EventType::NicMsix_t: {
        std::shared_ptr<nic_msix_pack> msix_p = nullptr;
        if (not obtain_pack_ptr<nic_msix_pack>(msix_p)) {
          std::cerr << "could not allocate eth_p" << std::endl;
          break;
        }

        if (msix_p->add_to_pack(event_ptr)) {
          added = true;
          if (msix_p->is_complete()) {
            co_yield msix_p;  // TODO: remove
            msix_p = nullptr;
          }
        }
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
