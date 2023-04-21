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

tar_task host_packer::produce() {
  src_task src = prod_.produce();

  bool pci_msix_desc_addr_before = false;
  event_t event_ptr = nullptr;
  std::shared_ptr<host_msix_pack> host_msix_p = nullptr;
  std::shared_ptr<generic_single_pack> gen_sin_pack = nullptr;
  std::shared_ptr<host_dma_pack> pending_dma = nullptr;
  bool added = false;

  while (src) {
    event_ptr = src.get();
    added = false;

    switch (event_ptr->getType()) {
      case EventType::HostCall_t: {
        if (not obtain_pack_ptr<host_call_pack>(pending_host_call_pack_,
                                                env_)) {
          std::cerr << "could not allocate pending_host_call_pack_"
                    << std::endl;
          break;
        }

        if (pending_host_call_pack_->add_to_pack(event_ptr)) {
          pci_msix_desc_addr_before = env_.is_pci_msix_desc_addr(event_ptr);
          added = true;

        } else if (pending_host_call_pack_->is_complete()) {
          co_yield pending_host_call_pack_;
          pending_host_call_pack_ = nullptr;

          if (not obtain_pack_ptr<host_call_pack>(pending_host_call_pack_,
                                                  env_)) {
            std::cerr << "found new syscall entry, could not allocate "
                         "pending_host_call_pack_"
                      << std::endl;
          }

          if (pending_host_call_pack_->add_to_pack(event_ptr)) {
            added = true;
          }
        }
        break;
      }

      case EventType::HostMmioW_t:
      case EventType::HostMmioR_t:
      case EventType::HostMmioImRespPoW_t:
      case EventType::HostMmioCW_t:
      case EventType::HostMmioCR_t: {
        if (not obtain_pack_ptr<host_mmio_pack>(pending_host_mmio_pack_, env_,
                                                pci_msix_desc_addr_before)) {
          std::cerr << "could not allocate pending_host_mmio_pack_"
                    << std::endl;
          break;
        }

        if (pending_host_mmio_pack_->add_to_pack(event_ptr)) {
          added = true;
          if (pending_host_mmio_pack_->is_complete()) {
            co_yield pending_host_mmio_pack_;
            pending_host_mmio_pack_ = nullptr;
          }

        } else if (is_type(event_ptr, EventType::HostMmioW_t) and
                   pending_host_mmio_pack_->pci_msix_desc_addr_before_) {
          pending_host_mmio_pack_->mark_as_done();
          co_yield pending_host_mmio_pack_;
          pending_host_mmio_pack_ = nullptr;
          if (not obtain_pack_ptr<host_mmio_pack>(pending_host_mmio_pack_, env_,
                                                  pci_msix_desc_addr_before)) {
            std::cerr << "could not allocate pending_host_mmio_pack_"
                      << std::endl;
            break;
          }

          if (pending_host_mmio_pack_->add_to_pack(event_ptr)) {
            added = true;
          }
        }
        break;
      }

      case EventType::HostDmaW_t:
      case EventType::HostDmaR_t:
      case EventType::HostDmaC_t: {
        pending_dma = iterate_add_erase<host_dma_pack>(pending_host_dma_packs_,
                                                       event_ptr);
        if (pending_dma) {
          added = true;
          if (pending_dma->is_complete()) {
            co_yield pending_dma;
          }
          break;
        }

        pending_dma = nullptr;
        if (not obtain_pack_ptr<host_dma_pack>(pending_dma, env_)) {
          std::cerr << "could not allocate pending_host_dma_pack_" << std::endl;
          break;
        }

        if (pending_dma->add_to_pack(event_ptr)) {
          added = true;
          pending_host_dma_packs_.push_back(pending_dma);
        }

        break;
      }

      case EventType::HostMsiX_t: {
        if (not obtain_pack_ptr<host_msix_pack>(host_msix_p, env_)) {
          std::cerr << "could not allocate host_mmio_p" << std::endl;
          break;
        }

        if (host_msix_p->add_to_pack(event_ptr)) {
          added = true;
          if (host_msix_p->is_complete()) {
            co_yield host_msix_p;
            host_msix_p = nullptr;
          }
        }
        break;
      }

      case EventType::HostPostInt_t:
      case EventType::HostClearInt_t: {
        if (not obtain_pack_ptr<host_int_pack>(pending_host_int_pack_, env_)) {
          std::cerr << "could not allocate pending_host_int_pack_" << std::endl;
          break;
        }

        if (pending_host_int_pack_->add_to_pack(event_ptr)) {
          added = true;
          if (pending_host_int_pack_->is_complete()) {
            co_yield pending_host_int_pack_;
            pending_host_int_pack_ = nullptr;
          }
        }
        break;
      }

      default:
        std::cout << "encountered non expected event ";
        if (event_ptr) {
          std::cout << *event_ptr << std::endl;
        } else {
          std::cout << "null" << std::endl;
        }
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
