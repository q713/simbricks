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

#ifndef SIMBRICKS_TRACE_EVENT_HOSTINT_PACK_H_
#define SIMBRICKS_TRACE_EVENT_HOSTINT_PACK_H_

#include "trace/analytics/config.h"
#include "trace/analytics/packs/pack.h"
#include "trace/events/events.h"

struct host_int_pack : public event_pack {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;

  event_t host_post_int_ = nullptr;
  event_t host_clear_int_ = nullptr;

  host_int_pack() : event_pack(pack_type::HOST_INT_PACK) {
  }

  ~host_int_pack() = default;

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
};

#endif  // SIMBRICKS_TRACE_EVENT_HOSTINT_PACK_H_