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

#ifndef SIMBRICKS_TRACE_EVENT_ETH_PACK_H_
#define SIMBRICKS_TRACE_EVENT_ETH_PACK_H_

#include "trace/analytics/packs/pack.h"
#include "trace/analytics/packs/callPack.h"
#include "trace/events/events.h"
#include "trace/env/traceEnvironment.h"

struct eth_pack : public event_pack {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;

  // NicTx or NicRx
  event_t tx_rx_ = nullptr;
  bool is_send_ = false;

  eth_pack(sim::trace::env::trace_environment &env) : event_pack(pack_type::ETH_PACK, env) {
  }

  ~eth_pack() = default;

  inline bool is_transmit() {
    return is_send_;
  }

  inline bool is_receive() {
    return not is_transmit();
  }

  bool add_if_triggered(pack_t pack_ptr) override {
    if (not potentially_triggered(pack_ptr)) {
      return false;
    }

    if (not tx_rx_) {
      return false;
    }

    if (is_send_ and is_type(pack_ptr, pack_type::ETH_PACK)) {
      std::shared_ptr<eth_pack> np =
          std::static_pointer_cast<eth_pack>(pack_ptr);
      if (not np->tx_rx_ or np->is_send_) {
        return false;
      }

      add_triggered(pack_ptr);
      return true;

    // NOTE:
    //  - after Tx we expect a dma write to indicate sending
    //  - after Rx we expect data writes and register info that something was sent
    } else if (is_type(pack_ptr, pack_type::DMA_PACK)) {
      add_triggered(pack_ptr);
      return true;
    }

    return false;
  }

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
};

#endif  // SIMBRICKS_TRACE_EVENT_ETH_PACK_H_