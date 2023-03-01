/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software withos restriction, including
 * withos limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHos WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, os OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_

#include <vector>
#include <set>
#include <map>
#include <optional>
#include <memory>
#include <list>

#include "trace/config.h"
#include "trace/corobelt/coroutine.h"
#include "trace/events/events.h"
#include "lib/utils/log.h"
#include "lib/utils/string_util.h"

using event_t = std::shared_ptr<Event>;
using task_t = sim::coroutine::task<void>;
using chan_t = sim::coroutine::unbuffered_single_chan<event_t>;
using msg_t = std::optional<event_t>;

class EventTypeStatistics : public sim::coroutine::pipe<event_t> {
 public:
  struct EventStat {
    uint64_t last_ts_;
    uint64_t first_ts_;
    uint64_t event_count_;
    const std::string name_;

    EventStat(const std::string name)
        : last_ts_(0), first_ts_(0), event_count_(0), name_(std::move(name)) {
    }

    friend std::ostream &operator<<(std::ostream &out, EventStat &statistic) {
      out << "typeinfo name:" << statistic.name_ << std::endl;
      out << "last_ts: " << std::to_string(statistic.last_ts_) << std::endl;
      out << "first_ts: " << std::to_string(statistic.first_ts_) << std::endl;
      out << "event_count: " << std::to_string(statistic.event_count_)
          << std::endl;
      return out;
    }
  };

 private:
  std::set<EventType> types_to_gather_statistic_;

  // TODO: map from event_type,simulator -> statistics
  // NOTE that this is also possible with the pipeline features by gathering
  // statistics before merging event streams of different simulators
  uint64_t total_event_count_;
  std::map<EventType, std::shared_ptr<EventStat>> statistics_by_type_;

  bool update_statistics(std::shared_ptr<Event> event_ptr) {
    EventType key = event_ptr->getType();
    std::shared_ptr<EventStat> statistic;
    const auto &statistics_search = statistics_by_type_.find(key);
    if (statistics_search == statistics_by_type_.end()) {
      statistic = std::make_shared<EventStat>(event_ptr->getName());
      if (statistic) {
        statistic->first_ts_ = event_ptr->timestamp_;
        auto success = statistics_by_type_.insert(
            std::make_pair(key, std::move(statistic)));
        if (!success.second) {
          return false;
        }
        statistic = success.first->second;
      } else {
        return false;
      }
    } else {
      statistic = statistics_search->second;
    }

    if (statistic == nullptr) {
      return false;
    }

    statistic->event_count_ = statistic->event_count_ + 1;
    statistic->last_ts_ = event_ptr->timestamp_;

    return true;
  }

 public:
  explicit EventTypeStatistics()
      : sim::coroutine::pipe<event_t>(), total_event_count_(0) {
  }

  explicit EventTypeStatistics(std::set<EventType> types_to_gather_statistic)
      : sim::coroutine::pipe<event_t>(),
        types_to_gather_statistic_(std::move(types_to_gather_statistic)),
        total_event_count_(0) {
  }

  ~EventTypeStatistics() = default;

  task_t process(chan_t *src_chan, chan_t *tar_chan) override {
    if (!src_chan || !tar_chan) {
      co_return;
    }

    event_t event;
    msg_t msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      event = msg.value();
      auto search = types_to_gather_statistic_.find(event->getType());
      if (types_to_gather_statistic_.empty() or
          search != types_to_gather_statistic_.end()) {
        if (not update_statistics(event)) {
#ifdef DEBUG_EVENT_
          DFLOGWARN("statistics for event with name %s could not be updated\n",
                    event->getName().c_str());
#endif
        }
      }
      total_event_count_ = total_event_count_ + 1;
    }

    co_return;
  }

  friend std::ostream &operator<<(std::ostream &out,
                                  EventTypeStatistics &eventTypeStatistics) {
    out << "EventTypeStatistics:" << std::endl;
    out << "a total of "
        << std::to_string(eventTypeStatistics.total_event_count_)
        << " events were counted" << std::endl;
    std::shared_ptr<EventStat> statistic_ptr;
    const std::map<EventType, std::shared_ptr<EventStat>> &statistics =
        eventTypeStatistics.getStatistics();
    if (statistics.empty()) {
      out << "no detailed statistics were gathered" << std::endl;
      return out;
    }

    for (auto it = statistics.begin(); it != statistics.end(); it++) {
      statistic_ptr = it->second;
      EventStat statistic(*statistic_ptr);
      out << std::endl;
      out << statistic;
      out << std::endl;
    }
    return out;
  }

  const std::map<EventType, std::shared_ptr<EventStat>> &getStatistics() {
    return statistics_by_type_;
  }

  std::optional<std::shared_ptr<EventStat>> getStatistic(EventType type) {
    auto statistic_search = statistics_by_type_.find(type);
    if (statistic_search != statistics_by_type_.end()) {
      return std::make_optional(statistic_search->second);
    }
    return std::nullopt;
  }
};

enum Stacks { KERNEL, NIC, SWITCH, NETWORK };

std::ostream &operator<<(std::ostream &out, Stacks s) {
  switch (s) {
    case KERNEL:
      out << "KERNEL";
      break;
    case NIC:
      out << "NIC";
      break;
    case SWITCH:
      out << "SWITCH";
      break;
    case NETWORK:
      out << "NETWORK";
      break;
    default:
      out << "UNDEFINED";
      break;
  }
  return out;
}

struct event_stream_tracer : public sim::coroutine::pipe<event_t> {
  struct event_pack;        // forward declaration
  struct event_pack_stack;  // forward declaration
  using pack_t = std::shared_ptr<event_pack>;
  using stack_t = std::shared_ptr<event_pack_stack>;
  using hostc_t = std::shared_ptr<HostCall>;
  using hostmmiow_t = std::shared_ptr<HostMmioW>;

  struct event_pack {
    std::vector<event_t> events_;

    event_pack() = default;

    friend std::ostream &operator<<(std::ostream &out, event_pack &pack) {
      out << std::endl;
      out << "Event-Pack: " << std::endl;
      for (event_t event : pack.events_) {
        out << *event;
      }
      out << std::endl;
      return out;
    }
  };

  struct event_pack_stack {
    
    friend std::ostream &operator<<(std::ostream &out,
                                    event_pack_stack &stack) {
      out << std::endl;
      out << std::endl;
      out << "Event-Pack-Stack:" << std::endl;
      for (auto pack : stack.event_packs_) {
        out << "From Stack: " << pack.first << std::endl;
        out << pack.second;
      }
      out << std::endl;
      out << std::endl;
      return out;
    }

    bool add_to_pack(Stacks stack, event_t event_ptr) {
      if (!event_ptr) {
        return false;
      }

      pack_t pack = nullptr;
      auto it = event_packs_.find(stack);
      if (it != event_packs_.end()) {
        pack = it->second;
      } else {
        pack = std::make_shared<event_pack>();
        if (!pack) {
          return false;
        }
      }

      pack->events_.push_back(event_ptr);
      return true;
    }

    private:
      std::unordered_map<Stacks, pack_t> event_packs_;
  };

  std::vector<stack_t> event_pack_stacks_;

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // TODO: add dump for yet unmatched events!!!!!!!!!!!!!!!!!!!!!
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  std::list<event_t> unmatched_events;

  task_t process(chan_t *src_chan, chan_t *tar_chan) override {
    if (not src_chan or not tar_chan) {
      co_return;
    }
    msg_t msg;
    event_t event_ptr = nullptr;
    stack_t cur_stack = nullptr;
    hostc_t host_call = nullptr;


    bool is_trace = false;
    bool exited_kernel = false;
    bool found_nic_driver_sym = false;
    bool is_netstack_related = false;
    bool is_yet_unmatched_event = false;
    std::vector<event_t> pending_host_mmio_actions;
    std::vector<event_t> pending_nic_dma_actions;
    while(src_chan->is_open() || !unmatched_events.empty()) {
      event_ptr = nullptr;
      if (!unmatched_events.empty()) {
        // we are in this case when we are not longer within a trace
        event_ptr = unmatched_events.front();
        unmatched_events.pop_front();
        is_yet_unmatched_event = true;
      } else {
        msg = co_await src_chan->read();
        if (msg) {
          event_ptr = msg.value();
        }
        is_yet_unmatched_event = false;
      }

      if (!event_ptr) {
        DLOGERR("no event left but one would be expected\n");
        co_return;
      }

      // find syscall entry entry point of a trace
      host_call = std::dynamic_pointer_cast<HostCall>(msg.value());
      // check if this is a syscall
      if (!is_trace and host_call and 0 == host_call->func_.compare("entry_SYSCALL_64")) {
        is_trace = true;
        cur_stack = std::make_shared<event_pack_stack>();
        if (!cur_stack or 
          !cur_stack->add_to_pack(Stacks::KERNEL, host_call)) {
          DLOGERR("could not add syscall entry event to current stack\n");  
          co_return;
        }
        continue;
      } 

      // when we are currently not in a trace we just pass the event on
      if (!is_trace) {
        DLOGWARN("found orpahend event\n");
        if (!co_await tar_chan->write(event_ptr)) {
          co_return;
        }
        continue;
      }

      // we are in a trace, now we built the trace
      if (is_type(event_ptr, EventType::HostCall_t)) {
        if (exited_kernel) {
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        host_call = std::static_pointer_cast<HostCall>(event_ptr);
        
        found_nic_driver_sym = found_nic_driver_sym or
          sim::trace::conf::I40E_DRIVER_FUNC_INDICATOR.contains(host_call->func_);
        
        is_netstack_related = is_netstack_related or found_nic_driver_sym or 
          sim::trace::conf::LINUX_NET_STACK_FUNC_INDICATOR.contains(host_call->func_);

        host_call = std::static_pointer_cast<HostCall>(event_ptr);
        exited_kernel = exited_kernel or (0 == host_call->func_.compare("syscall_return_via_sysret"));
        cur_stack->add_to_pack(Stacks::KERNEL, event_ptr);
        continue;
      }

      if (!is_netstack_related or !found_nic_driver_sym) {
        DLOGWARN("found non host call outside of networking stack\n");
        if (not is_yet_unmatched_event) {
          unmatched_events.push_back(event_ptr);
        }
        continue;
      }

      if (is_type(event_ptr, EventType::HostMmioW_t)) {
        // mmio access to the nic to issue action
        if (not pending_host_mmio_actions.empty()) {
          DLOGWARN("Non yet completed host mmio actions to issue nic action\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_host_mmio_actions.push_back(event_ptr);

      }
      else if (is_type(event_ptr, EventType::HostMmioImRespPoW_t)) {
        if (pending_host_mmio_actions.size() != 1) {
          DLOGWARN("Non yet expected HostMmioImRespPoW_t found\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        auto mmiow = std::static_pointer_cast<HostMmioW>(pending_host_mmio_actions.front());
        if (event_ptr->timestamp_ != mmiow->timestamp_) {
          DLOGWARN("On mmiow_t did not follow immediate response\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_host_mmio_actions.push_back(event_ptr);
 
      }
      else if (is_type(event_ptr, EventType::NicMmioW_t)) {
        auto ne = std::static_pointer_cast<NicMmioW>(event_ptr);
        if (pending_host_mmio_actions.size() != 2) {
          DLOGWARN("found NicMmioW_t but not two mmio events beforehand\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        auto he = std::dynamic_pointer_cast<HostMmioW>(pending_host_mmio_actions.front());
        if (!he) {
          DLOGWARN("found NicMmioW_t but first pending is no HostMmioW\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        
        // TODO: optimize this
        std::stringstream he_adrr;
        he_adrr << std::hex << he->addr_;
        std::stringstream ne_off;
        ne_off << std::hex << ne->off_;
        if (!sim_string_utils::ends_with(he_adrr.str(), ne_off.str()) or he->size_ != ne->len_) {
          DLOGWARN("NicMmioW_t does not match preceding HostMmioW\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_host_mmio_actions.push_back(event_ptr);

      } 
      else if (is_type(event_ptr, EventType::HostMmioCW_t)) {
        auto he = std::static_pointer_cast<HostMmioCW>(event_ptr);
        if (pending_host_mmio_actions.size() != 3) {
          DLOGWARN("found HostMmioCW_t but not three mmio (two host one nic) events beforehand\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        auto fhe = std::dynamic_pointer_cast<HostMmioW>(pending_host_mmio_actions.front());
        if (!fhe || he->id_ != fhe->id_) {
          DLOGWARN("found HostMmioCW_t with an unexpected id\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        
        pending_host_mmio_actions.push_back(he);

      }
      else if (is_type(event_ptr, EventType::NicDmaI_t)) {
        if (pending_host_mmio_actions.size() != 3) {
          DLOGWARN("found NicDmaI_t but no mmio register write to device captured\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_nic_dma_actions.push_back(event_ptr);

      }
      else if (is_type(event_ptr, EventType::NicDmaEx_t)) {
        if (pending_nic_dma_actions.size() != 1) {
          DLOGWARN("found NicDmaEx_t before issuing this operation\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        auto i = std::static_pointer_cast<NicDmaI>(pending_nic_dma_actions.front());
        auto e = std::static_pointer_cast<NicDmaEx>(event_ptr);
        if (i->id_ != e->id_) {
          DLOGWARN("found NicDmaEx_t that doesnt match issue id\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_nic_dma_actions.push_back(event_ptr);

      }
      else if (is_type(event_ptr, EventType::HostDmaR_t)) {
        if (pending_nic_dma_actions.size() != 2) {
          DLOGWARN("found HostDmaR_t before nic site execution of this operation\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        auto e = std::static_pointer_cast<NicDmaEx>(pending_nic_dma_actions.back());
        auto r = std::static_pointer_cast<HostDmaR>(event_ptr);
        if (r->addr_ != e->addr_) {
          DLOGWARN("found HostDmaR_t with wrong address\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_nic_dma_actions.push_back(event_ptr);

      }
      else if (is_type(event_ptr, EventType::HostDmaC_t)) {
        if (pending_nic_dma_actions.size() != 3) {
          DLOGWARN("found HostDmaC_t before HostDmaR_t\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        auto r = std::static_pointer_cast<HostDmaR>(pending_nic_dma_actions.back());
        auto c = std::static_pointer_cast<HostDmaC>(event_ptr);
        if (c->id_ != r->id_) {
          DLOGWARN("found HostDmaC_t with non matching id\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_nic_dma_actions.push_back(event_ptr);

      }
      else if (is_type(event_ptr, EventType::NicDmaCR_t)) {
        if (pending_nic_dma_actions.size() != 4) {
          DLOGWARN("found NicDmaCR_t before HostDmaC_t\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }

        auto i = std::static_pointer_cast<NicDmaI>(pending_nic_dma_actions.front());
        auto c = std::static_pointer_cast<NicDmaCR>(event_ptr);
        if (c->id_ != i->id_) {
          DLOGWARN("found NicDmaCR with non matching id\n");
          if (not is_yet_unmatched_event) {
            unmatched_events.push_back(event_ptr);
          }
          continue;
        }
        pending_nic_dma_actions.push_back(event_ptr);

      } 
      else {
        DLOGWARN("found orphaned event for which it is unclear what to do, it is passed\n");
        if (!co_await tar_chan->write(event_ptr)) {
          DLOGERR("couldnt write evnt to target channel\n");
          co_return;
        }
        continue;
      }

      if (pending_host_mmio_actions.size() == 4) {
          for (auto e : pending_host_mmio_actions) {
            cur_stack->add_to_pack(Stacks::KERNEL, e);
          }
          pending_host_mmio_actions.clear();
      }
      if (pending_nic_dma_actions.size() == 5) {
        for (auto e : pending_nic_dma_actions) {
          cur_stack->add_to_pack(Stacks::NIC, e);
        }
        pending_nic_dma_actions.clear();
      }

      if (exited_kernel 
          and pending_host_mmio_actions.size() == 4
          and pending_nic_dma_actions.size() == 5) {
        DLOGIN("found one stack to finish\n");
        // finish up this stack
        is_trace = false;
        exited_kernel = false;
        found_nic_driver_sym = false;
        event_pack_stacks_.push_back(cur_stack);
        cur_stack = nullptr;
      }
    }
  }

  explicit event_stream_tracer() : sim::coroutine::pipe<event_t>() {
  }

  ~event_stream_tracer() = default;

  friend std::ostream &operator<<(std::ostream &out,
                                  event_stream_tracer &tracer) {
    out << std::endl;
    out << std::endl;
    out << "event_stream_tracer" << std::endl;
    for (stack_t stack : tracer.event_pack_stacks_) {
      out << *stack;
    }
    out << std::endl;
    out << std::endl;
    return out;
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
