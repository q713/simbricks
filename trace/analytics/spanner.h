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
#include "queue.h"
#include "span.h"
#include "traceEnvironment.h"
#include "tracer.h"

#ifndef SIMBRICKS_TRACE_spanER_H_
#define SIMBRICKS_TRACE_spanER_H_

struct spanner : public sim::corobelt::consumer<std::shared_ptr<Event>> {
  uint64_t id_;

  tracer &tracer_;

  // Override this method!
  virtual sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<std::shared_ptr<Event>> *producer_task)
      override = 0;

  spanner(tracer &t)
      : id_(trace_environment::get_next_spanner_id()), tracer_(t) {
  }

  inline uint64_t get_id() {
    return id_;
  }

  bool ends_with_offset(uint64_t addr, uint64_t off) {
    size_t lz = std::__countl_zero(off);
    uint64_t mask = lz == 64 ? 0xffffffffffffffff : (1 << (64 - lz)) - 1;
    uint64_t check = addr & mask;
    return check == off;
  }

  // template <class st, class... Args>
  // std::shared_ptr<st> register_span(std::shared_ptr<event_span> parent,
  //                                   Args &&...args) {
  //   if (not parent) {
  //     std::cout << "try to create a new span without parent" << std::endl;
  //     return {};
  //   }
  //
  //  auto result =
  //      tracer_.rergister_new_span<st>(parent->get_trace_id(), args...);
  //  if (not result) {
  //    std::cerr << "could not allocate new span" << std::endl;
  //    return {};
  //  }
  //
  //  result->set_parent(parent);
  //  return result;
  //}

  template <class pt>
  std::shared_ptr<pt> iterate_add_erase(std::list<std::shared_ptr<pt>> &pending,
                                        std::shared_ptr<Event> event_ptr) {
    std::shared_ptr<pt> pending_span = nullptr;

    for (auto it = pending.begin(); it != pending.end(); it++) {
      pending_span = *it;
      if (pending_span and pending_span->add_to_span(event_ptr)) {
        if (pending_span->is_complete()) {
          pending.erase(it);
        }
        return pending_span;
      }
    }

    return nullptr;
  }
};

struct host_spanner : public spanner {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<std::shared_ptr<Event>> *producer_task)
      override;

  host_spanner(tracer &t, context_queue &queue, bool is_client)
      : spanner(t), queue_(queue), is_client_(is_client) {
  }

 protected:
  bool create_trace_starting_span(uint64_t parser_id);

  bool handel_call(std::shared_ptr<Event> event_ptr);

  bool handel_mmio(std::shared_ptr<Event> event_ptr);

  bool handel_dma(std::shared_ptr<Event> event_ptr);

  bool handel_msix(std::shared_ptr<Event> event_ptr);

  bool handel_int(std::shared_ptr<Event> event_ptr);

 private:
  context_queue &queue_;

  bool is_client_;

  size_t expected_xmits_ = 0;
  bool found_transmit_ = false;
  bool found_receive_ = false;
  bool pci_msix_desc_addr_before_ = false;
  std::shared_ptr<host_call_span> pending_host_call_span_ = nullptr;
  std::shared_ptr<host_int_span> pending_host_int_span_ = nullptr;
  std::list<std::shared_ptr<host_dma_span>> pending_host_dma_spans_;
  std::shared_ptr<host_mmio_span> pending_host_mmio_span_ = nullptr;
};

struct nic_spanner : public spanner {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<std::shared_ptr<Event>> *producer_task);

  nic_spanner(tracer &t, context_queue &host_queue,
              context_queue &network_queue)
      : spanner(t), host_queue_(host_queue), network_queue_(network_queue) {
  }

 protected:

  bool handel_mmio(std::shared_ptr<Event> event_ptr);

  bool handel_dma(std::shared_ptr<Event> event_ptr);

  bool handel_txrx(std::shared_ptr<Event> event_ptr);

  bool handel_msix(std::shared_ptr<Event> event_ptr);

 private:
  context_queue &host_queue_;
  context_queue &network_queue_;

  std::shared_ptr<context> last_host_context_ = nullptr;
  std::shared_ptr<event_span> last_completed_ = nullptr;

  std::list<std::shared_ptr<nic_dma_span>> pending_nic_dma_spans_;
};

#endif  // SIMBRICKS_TRACE_spanER_H_
