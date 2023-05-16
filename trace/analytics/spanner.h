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
#include "exception.h"
#include "queue.h"
#include "span.h"
#include "traceEnvironment.h"
#include "tracer.h"

#ifndef SIMBRICKS_TRACE_spanER_H_
#define SIMBRICKS_TRACE_spanER_H_

struct Spanner : public consumer<std::shared_ptr<Event>> {
  uint64_t id_;

  tracer &tracer_;

  // Override this method!
  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan)
  override = 0;

  Spanner(tracer &t)
      : id_(trace_environment::get_next_spanner_id()), tracer_(t) {
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

  HostSpanner(tracer &t, ContextQueue &queue, bool is_client)
      : Spanner(t), queue_(queue), is_client_(is_client) {
  }

  static auto create(tracer &t, ContextQueue &queue, bool is_client) {
    auto spanner = std::make_shared<HostSpanner>(t, queue, is_client);
    throw_if_empty(spanner, spanner_is_null);
    return spanner;
  }

  protected:
    bool create_trace_starting_span (uint64_t parser_id);

    bool handel_call (std::shared_ptr<Event> &event_ptr);

    bool handel_mmio (std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Event> &event_ptr);

    bool handel_dma (std::shared_ptr<concurrencpp::executor> resume_executor,
                     std::shared_ptr<Event> &event_ptr);

    bool handel_msix (std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Event> &event_ptr);

    bool handel_int (std::shared_ptr<Event> &event_ptr);

  private:
    ContextQueue &queue_;

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

struct NicSpanner : public Spanner
  {
    concurrencpp::result<void> consume (
            std::shared_ptr<concurrencpp::executor> resume_executor,
            std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan) override;

    NicSpanner (tracer &t, ContextQueue &host_queue,
                ContextQueue &network_queue)
            : Spanner (t), host_queue_ (host_queue),
              network_queue_ (network_queue)
    {
    }

    static auto create(tracer &t, ContextQueue &host_queue, ContextQueue &network_queue) {
      auto spanner = std::make_shared<NicSpanner>(t, host_queue, network_queue);
      throw_if_empty(spanner, spanner_is_null);
      return spanner;
    }

  protected:

    bool handel_mmio (std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Event> &event_ptr);

    bool handel_dma (std::shared_ptr<concurrencpp::executor> resume_executor,
                     std::shared_ptr<Event> &event_ptr);

    bool handel_txrx (std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Event> &event_ptr);

    bool handel_msix (std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Event> &event_ptr);

  private:
    ContextQueue &host_queue_;
    ContextQueue &network_queue_;

    std::shared_ptr<Context> last_host_context_ = nullptr;
    std::shared_ptr<event_span> last_completed_ = nullptr;

    std::list<std::shared_ptr<nic_dma_span>> pending_nic_dma_spans_;
  };

#endif  // SIMBRICKS_TRACE_spanER_H_
