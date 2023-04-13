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

#ifndef SIMBRICKS_TRACE_EVENT_DMA_PACK_H_
#define SIMBRICKS_TRACE_EVENT_DMA_PACK_H_

#include <vector>
#include <iostream>
#include <memory>

#include "trace/analytics/packs/pack.h"
#include "trace/events/events.h"
#include "trace/env/traceEnvironment.h"

struct dma_pack : public event_pack {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;

  // NicDmaI_t
  event_t dma_issue_ = nullptr;
  // NicDmaEx_t
  event_t nic_dma_execution_ = nullptr;
  // HostDmaW_t or HostDmaR_t
  event_t host_dma_execution_ = nullptr;
  bool is_read_ = true;
  // HostDmaC_t
  event_t host_dma_completion_ = nullptr;
  // NicDmaCW_t or NicDmaCR_t
  event_t nic_dma_completion_ = nullptr;

  // NOTE: this info is only valid once the pack is complete
  inline bool is_read() {
    return is_read_;
  }

  // NOTE: this info is only valid once the pack is complete
  inline bool is_write() {
    return not is_read();
  }

  dma_pack(sim::trace::env::trace_environment &env) : event_pack(pack_type::DMA_PACK, env) {
  }

  ~dma_pack() = default;

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
        if (not dma_issue_ /*or not nic_dma_execution_*/) {
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
        if (not dma_issue_ /*or not nic_dma_execution_*/ or
            not host_dma_execution_) {
          return false;
        }

        auto exec =
            std::static_pointer_cast<HostAddrSizeOp>(host_dma_execution_);
        auto comp = std::static_pointer_cast<HostDmaC>(event_ptr);
        if (exec->id_ != comp->id_) {
          return false;
        }

        host_dma_completion_ = event_ptr;
        break;
      }

      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t: {
        if (not dma_issue_ /*or not nic_dma_execution_*/ or
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
};

#endif // SIMBRICKS_TRACE_EVENT_DMA_PACK_H_