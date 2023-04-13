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

#ifndef SIMBRICKS_TRACE_EVENT_MSIX_PACK_H_
#define SIMBRICKS_TRACE_EVENT_MSIX_PACK_H_

#include <memory>

#include "trace/analytics/packs/pack.h"
#include "trace/events/events.h"
#include "trace/env/traceEnvironment.h"

struct msix_pack : public event_pack {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;

  event_t nic_msix_ = nullptr;
  event_t host_msix_ = nullptr;

  msix_pack(sim::trace::env::trace_environment &env) : event_pack(pack_type::MSIX_PACK, env) {
  }

  ~msix_pack() = default;

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
};

#endif  // SIMBRICKS_TRACE_EVENT_MSIX_PACK_H_