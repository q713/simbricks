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
#include <utility>

#include "corobelt/corobelt.h"
#include "events/events.h"
#include "util/exception.h"
#include "analytics/context.h"
#include "analytics/span.h"
#include "env/traceEnvironment.h"
#include "analytics/tracer.h"

#ifndef SIMBRICKS_TRACE_spanER_H_
#define SIMBRICKS_TRACE_spanER_H_

struct Spanner : public consumer<std::shared_ptr<Event>> {
  uint64_t id_;

  Tracer &tracer_;

  // Override this method!
  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan)
  override = 0;

  explicit Spanner(Tracer &tra)
      : id_(trace_environment::get_next_spanner_id()), tracer_(tra) {
  }

  inline uint64_t get_id() const {
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

  template<class St>
  std::shared_ptr<St>
  iterate_add_erase(std::list<std::shared_ptr<St>> &pending,
                    std::shared_ptr<Event> event_ptr) {
    std::shared_ptr<St> pending_span = nullptr;

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

struct HostSpanner : public Spanner {
  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan) override;

  explicit HostSpanner(Tracer &tra,
                       std::shared_ptr<Channel<std::shared_ptr<Context>>> to_nic,
                       std::shared_ptr<Channel<std::shared_ptr<Context>>> from_nic, bool is_client)
      : Spanner(tra), to_nic_queue_(to_nic), from_nic_queue_(from_nic), is_client_(is_client) {
    throw_if_empty(to_nic, queue_is_null);
    throw_if_empty(from_nic, queue_is_null);
  }

 private:
  bool create_trace_starting_span(uint64_t parser_id);

  concurrencpp::lazy_result<bool> handel_call(std::shared_ptr<concurrencpp::executor> resume_executor,
                                              std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_mmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                                              std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_dma(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_msix(std::shared_ptr<concurrencpp::executor> resume_executor,
                                              std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_int(std::shared_ptr<Event> &event_ptr);

  std::shared_ptr<Channel<std::shared_ptr<Context>>> from_nic_queue_;
  std::shared_ptr<Channel<std::shared_ptr<Context>>> to_nic_queue_;

  bool is_client_;

  size_t expected_xmits_ = 0;
  bool found_transmit_ = false;
  bool found_receive_ = false;
  bool pci_write_before_ = false;
  std::shared_ptr<HostCallSpan> pending_host_call_span_ = nullptr;
  std::shared_ptr<HostIntSpan> pending_host_int_span_ = nullptr;
  std::shared_ptr<HostMsixSpan> pending_host_msix_span_ = nullptr;
  std::list<std::shared_ptr<HostDmaSpan>> pending_host_dma_spans_;
  std::list<std::shared_ptr<HostMmioSpan>> pending_host_mmio_spans_;
};

struct NicSpanner : public Spanner {
  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan) override;

  explicit NicSpanner(Tracer &tra, std::shared_ptr<Channel<std::shared_ptr<Context>>> to_network,
                      std::shared_ptr<Channel<std::shared_ptr<Context>>> from_network,
                      std::shared_ptr<Channel<std::shared_ptr<Context>>> to_host,
                      std::shared_ptr<Channel<std::shared_ptr<Context>>> from_host)
      : Spanner(tra),
        to_network_queue_(to_network),
        from_network_queue_(from_network),
        to_host_queue_(to_host),
        from_host_queue_(from_host) {

    throw_if_empty(to_network, queue_is_null);
    throw_if_empty(from_network, queue_is_null);
    throw_if_empty(to_host, queue_is_null);
    throw_if_empty(from_host, queue_is_null);
  }

 private:

  concurrencpp::lazy_result<bool> handel_mmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                                              std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_dma(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_txrx(std::shared_ptr<concurrencpp::executor> resume_executor,
                                              std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> handel_msix(std::shared_ptr<concurrencpp::executor> resume_executor,
                                              std::shared_ptr<Event> &event_ptr);

  std::shared_ptr<Channel<std::shared_ptr<Context>>> to_network_queue_;
  std::shared_ptr<Channel<std::shared_ptr<Context>>> from_network_queue_;
  std::shared_ptr<Channel<std::shared_ptr<Context>>> to_host_queue_;
  std::shared_ptr<Channel<std::shared_ptr<Context>>> from_host_queue_;

  std::shared_ptr<Context> last_host_context_ = nullptr;
  std::shared_ptr<EventSpan> last_completed_ = nullptr;

  std::list<std::shared_ptr<NicDmaSpan>> pending_nic_dma_spans_;
};

#endif  // SIMBRICKS_TRACE_spanER_H_
