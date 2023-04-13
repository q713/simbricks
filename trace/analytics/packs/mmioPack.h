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

#ifndef SIMBRICKS_TRACE_EVENT_MMIO_PACK_H_
#define SIMBRICKS_TRACE_EVENT_MMIO_PACK_H_

#include <memory>

#include "trace/analytics/packs/ethPack.h"
#include "trace/analytics/packs/pack.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/events.h"

struct mmio_pack : public event_pack {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;

  // issue, either host_mmio_w_ or host_mmio_r_
  event_t host_mmio_issue_ = nullptr;
  bool is_read_ = false;
  event_t host_msi_read_resp_ = nullptr;
  bool pci_msix_desc_addr_before_;
  event_t im_mmio_resp_ = nullptr;
  // nic action nic_mmio_w_ or nic_mmio_r_
  event_t action_ = nullptr;
  // completion, either host_mmio_cw_ or host_mmio_cr_
  event_t completion_ = nullptr;

  explicit mmio_pack(bool pci_msix_desc_addr_before,
                     sim::trace::env::trace_environment &env)
      : event_pack(pack_type::MMIO_PACK, env),
        pci_msix_desc_addr_before_(pci_msix_desc_addr_before) {
  }

  ~mmio_pack() = default;

  // NOTE: this info is only valid once the pack is complete
  inline bool is_read() {
    return is_read_;
  }

  // NOTE: this info is only valid once the pack is complete
  inline bool is_write() {
    return not is_read();
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

          auto issue =
              std::static_pointer_cast<HostAddrSizeOp>(host_mmio_issue_);
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
};

#endif  // SIMBRICKS_TRACE_EVENT_MMIO_PACK_H_