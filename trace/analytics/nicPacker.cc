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

#include "trace/analytics/packer.h"

using pack_t = std::shared_ptr<event_pack>;
using event_t = std::shared_ptr<Event>;
using src_task = sim::corobelt::yield_task<event_t>;
using tar_task = sim::corobelt::yield_task<pack_t>;

tar_task nic_packer::produce() {
  src_task src = prod_.produce();

  event_t event_ptr = nullptr;
  std::shared_ptr<nic_dma_pack> pending_dma = nullptr;
  bool added = false;

  while (src) {
    event_ptr = src.get();
    added = false;

    switch (event_ptr->getType()) {
      case EventType::NicMmioW_t:
      case EventType::NicMmioR_t: {
        std::shared_ptr<nic_mmio_pack> mmio_p = nullptr;
        if (not obtain_pack_ptr<nic_mmio_pack>(mmio_p, env_)) {
          std::cerr << "could not allocate mmio_p" << std::endl;
          break;
        }

        if (mmio_p->add_to_pack(event_ptr)) {
          added = true;
          if (mmio_p->is_complete()) {
            co_yield mmio_p;
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
            co_yield pending_dma;
          }
          break;
        }

        pending_dma = nullptr;
        if (not obtain_pack_ptr<nic_dma_pack>(pending_dma, env_)) {
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
        if (not obtain_pack_ptr<nic_eth_pack>(eth_p, env_)) {
          std::cerr << "could not allocate eth_p" << std::endl;
          break;
        }

        if (eth_p->add_to_pack(event_ptr)) {
          added = true;
          if (eth_p->is_complete()) {
            co_yield eth_p;
            eth_p = nullptr;
          }
        }
        break;
      }

      case EventType::NicMsix_t: {
        std::shared_ptr<nic_msix_pack> msix_p = nullptr;
        if (not obtain_pack_ptr<nic_msix_pack>(msix_p, env_)) {
          std::cerr << "could not allocate eth_p" << std::endl;
          break;
        }

        if (msix_p->add_to_pack(event_ptr)) {
          added = true;
          if (msix_p->is_complete()) {
            co_yield msix_p;
            msix_p = nullptr;
          }
        }
        break;
      }

      default: {
        std::cout
            << "add generic single pack for yet non specially treated event"
            << std::endl;
        std::shared_ptr<generic_single_pack> gen_sin_pack = nullptr;
        if (not obtain_pack_ptr<generic_single_pack>(gen_sin_pack, env_)) {
          std::cerr << "could not allocate gen_sin_pack" << std::endl;
          break;
        }

        if (gen_sin_pack->add_to_pack(event_ptr)) {
          added = true;
          if (gen_sin_pack->is_complete()) {
            co_yield gen_sin_pack;
            gen_sin_pack = nullptr;
          }
        }
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
