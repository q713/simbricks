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

#include "sync/corobelt.h"
#include "events/events.h"
#include "util/exception.h"
#include "analytics/context.h"
#include "analytics/span.h"
#include "env/traceEnvironment.h"
#include "analytics/tracer.h"
#include "analytics/timer.h"
#include "parser/parser.h"

#ifndef SIMBRICKS_TRACE_SPANNER_H_
#define SIMBRICKS_TRACE_SPANNER_H_

struct Spanner : public consumer<std::shared_ptr<Event>> {
  TraceEnvironment &trace_environment_;
  uint64_t id_;
  std::string name_;
  Tracer &tracer_;

  using ExecutorT = std::shared_ptr<concurrencpp::executor>;
  using EventT = std::shared_ptr<Event>;
  using ChannelT = std::shared_ptr<CoroChannel<std::shared_ptr<Context>>>;
  using LazyResultT = concurrencpp::lazy_result<bool>;
  using HandlerT = std::function<LazyResultT(ExecutorT, EventT &)>;
  std::unordered_map<EventType, HandlerT> handler_;

  concurrencpp::result<void> consume(ExecutorT resume_executor, std::shared_ptr<CoroChannel<EventT>> src_chan) override;

  explicit Spanner(TraceEnvironment &trace_environment,
                   std::string &&name,
                   Tracer &tra)
      : trace_environment_(trace_environment),
        id_(trace_environment_.GetNextSpannerId()),
        name_(name),
        tracer_(tra) {
  }

  explicit Spanner(TraceEnvironment &trace_environment,
                   std::string &&name,
                   Tracer &tra,
                   std::unordered_map<EventType, HandlerT> &&handler)
      : trace_environment_(trace_environment),
        id_(trace_environment_.GetNextSpannerId()),
        name_(name),
        tracer_(tra),
        handler_(handler) {
  }

  void RegisterHandler(EventType type, HandlerT &&handler) {
    auto it_suc = handler_.insert({type, handler});
    throw_on(not it_suc.second,
             "Spanner::RegisterHandler Could not insert new handler",
             source_loc::current());
  }

  inline uint64_t GetId() const {
    return id_;
  }

  template<class St>
  std::shared_ptr<St>
  iterate_add_erase(std::list<std::shared_ptr<St>> &pending,
                    std::shared_ptr<Event> event_ptr) {
    std::shared_ptr<St> pending_span = nullptr;

    for (auto it = pending.begin(); it != pending.end(); it++) {
      pending_span = *it;
      if (pending_span and pending_span->AddToSpan(event_ptr)) {
        if (pending_span->IsComplete()) {
          pending.erase(it);
        }
        return pending_span;
      }
    }

    return nullptr;
  }
};

struct HostSpanner : public Spanner {

  explicit HostSpanner(TraceEnvironment &trace_environment,
                       std::string &&name,
                       Tracer &tra,
                       std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_nic,
                       std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic,
                       std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_receives);

 private:
  concurrencpp::lazy_result<void> FinishPendingSpan(std::shared_ptr<concurrencpp::executor> resume_executor);

  concurrencpp::lazy_result<bool> CreateTraceStartingSpan(std::shared_ptr<concurrencpp::executor> resume_executor,
                                                          std::shared_ptr<Event> &starting_event,
                                                          bool fragmented);

  concurrencpp::lazy_result<bool> HandelCall(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  // NOTE: these two functions will push mmio expectations to the NIC site!!!
  concurrencpp::lazy_result<bool> HandelMmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);
  concurrencpp::lazy_result<bool> HandelPci(std::shared_ptr<concurrencpp::executor> resume_executor,
                                            std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                                            std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> HandelMsix(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> HandelInt(std::shared_ptr<concurrencpp::executor> resume_executor,
                                            std::shared_ptr<Event> &event_ptr);

  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_nic_receives_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_nic_queue_;

  bool pci_write_before_ = false;
  std::shared_ptr<HostCallSpan> last_trace_starting_span_ = nullptr;
  std::shared_ptr<HostCallSpan> pending_host_call_span_ = nullptr;
  std::shared_ptr<HostIntSpan> pending_host_int_span_ = nullptr;
  std::shared_ptr<HostMsixSpan> pending_host_msix_span_ = nullptr;
  std::list<std::shared_ptr<HostDmaSpan>> pending_host_dma_spans_;
  std::list<std::shared_ptr<HostMmioSpan>> pending_host_mmio_spans_;
  std::shared_ptr<HostPciSpan> pending_pci_span_;
};

struct NicSpanner : public Spanner {

  explicit NicSpanner(TraceEnvironment &trace_environment,
                      std::string &&name,
                      Tracer &tra,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_network,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_network,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_host,
                      std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_receives);

 private:

  concurrencpp::lazy_result<bool> HandelMmio(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> HandelDma(std::shared_ptr<concurrencpp::executor> resume_executor,
                                            std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> HandelTxrx(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  concurrencpp::lazy_result<bool> HandelMsix(std::shared_ptr<concurrencpp::executor> resume_executor,
                                             std::shared_ptr<Event> &event_ptr);

  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_network_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_network_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_host_queue_;
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> to_host_receives_;

  std::shared_ptr<Context> last_host_context_ = nullptr;
  std::shared_ptr<EventSpan> last_causing_ = nullptr;

  std::list<std::shared_ptr<NicDmaSpan>> pending_nic_dma_spans_;
};

struct NetworkSpanner : public Spanner {

  class NodeDeviceToChannelMap {
    using ChanT = std::shared_ptr<CoroChannel<std::shared_ptr<Context>>>;
    std::map<std::pair<int, int>, ChanT> mapping_;

    void AddMapping(const std::pair<int, int> &key, const ChanT &channel) {
      auto suc = mapping_.insert({key, channel});
      throw_on_false(suc.second, "NetworkSpanner ip address already mapped",
                     source_loc::current());
    }

    ChanT GetChannel(const std::pair<int, int> &node_device) const {
      auto iter = mapping_.find(node_device);
      if (iter != mapping_.end()) {
        return iter->second;
      }
      return nullptr;
    }

   public:
    explicit NodeDeviceToChannelMap() = default;

    void AddMapping(int node, int device, const ChanT &channel) {
      auto key = std::make_pair(node, device);
      AddMapping(key, channel);
    }

    ChanT GetValidChannel(int node, int device) const {
      auto result = GetChannel(std::make_pair(node, device));
      throw_if_empty(result, TraceException::kChannelIsNull, source_loc::current());
      return result;
    }
  };

  class NodeDeviceFilter {
    std::set<std::pair<int, int>> interesting_node_device_pairs_;

   public:
    explicit NodeDeviceFilter() = default;

    void AddNodeDevice(const std::pair<int, int> &node_device) {
      interesting_node_device_pairs_.insert(node_device);
    }

    void AddNodeDevice(int node, int device) {
      interesting_node_device_pairs_.insert({node, device});
    }

    void AddNodeDevice(const std::shared_ptr<NetworkEvent> &event) {
      if (not event) {
        return;
      }
      const int node = event->GetNode();
      const int device = event->GetDevice();
      interesting_node_device_pairs_.insert({node, device});
    }

    bool IsInterestingNodeDevice(int node, int device) const {
      return interesting_node_device_pairs_.contains({node, device});
    }

    bool IsInterestingNodeDevice(const std::shared_ptr<NetworkEvent> &event) const {
      if (not event) {
        return false;
      }
      const int node = event->GetNode();
      const int device = event->GetDevice();
      return IsInterestingNodeDevice(node, device);
    }

    bool IsNotInterestingNodeDevice(const std::shared_ptr<NetworkEvent> &event) const {
      return not IsInterestingNodeDevice(event);
    }
  };

  explicit NetworkSpanner(TraceEnvironment &trace_environment,
                          std::string &&name,
                          Tracer &tra,
                          ChannelT from_host_,
                          const NodeDeviceToChannelMap &to_host_channels,
                          const NodeDeviceFilter &node_device_filter);

 private:

  concurrencpp::lazy_result<bool> HandelNetworkEvent(ExecutorT resume_executor,
                                                     std::shared_ptr<Event> &event_ptr);

  // TODO: may need this to be a vector as well
  std::shared_ptr<NetDeviceSpan> last_finished_device_span_ = nullptr;
  std::shared_ptr<NetDeviceSpan> current_device_span_ = nullptr;

  // TODO: make these vectors --> mechanism needed to decide to which host to send to
  std::shared_ptr<CoroChannel<std::shared_ptr<Context>>> from_host_;
  const NodeDeviceToChannelMap &to_host_channels_;
  const NodeDeviceFilter &node_device_filter_;
};

#endif  // SIMBRICKS_TRACE_SPANNER_H_