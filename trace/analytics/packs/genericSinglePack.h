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

#ifndef SIMBRICKS_TRACE_EVENT_GENERIC_PACK_H_
#define SIMBRICKS_TRACE_EVENT_GENERIC_PACK_H_

#include "trace/analytics/packs/pack.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/events.h"

struct single_event_pack : public event_pack {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;
  
  event_t event_p_ = nullptr;

  single_event_pack(sim::trace::env::trace_environment &env)
      : event_pack(pack_type::SE_PACK, env) {
  }

  virtual bool add_on_match(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    if (event_p_) {
      return false;
    }

    event_p_ = event_ptr;
    is_pending_ = false;
    add_to_pack(event_ptr);
    return true;
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_GENERIC_PACK_H_