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

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_

#include <bit>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/config.h"
#include "trace/corobelt/coroutine.h"
#include "trace/events/events.h"

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

// forward declarations + type aliases
struct event_pack;
struct call_pack;
struct mmio_pack;
struct dma_pack;
struct eth_pack;
struct single_event_pack;
using pack_t = std::shared_ptr<event_pack>;
using callp_t = std::shared_ptr<call_pack>;
using mmiop_t = std::shared_ptr<mmio_pack>;
using dmap_t = std::shared_ptr<dma_pack>;

struct trace;
using trace_t = std::shared_ptr<trace>;

void write_ident(std::ostream &out, unsigned ident) {
  if (ident == 0)
    return;

  for (size_t i = 0; i < ident; i++) {
    out << "\t";
  }
}

inline uint64_t get_pack_id() {
  static uint64_t next_id = 0;
  return next_id++;
}

// TODO: maybe add a trigger type
enum pack_type { CALL_PACK, DMA_PACK, MMIO_PACK, SE_PACK, ETH_PACK };

inline std::ostream &operator<<(std::ostream &os, pack_type t) {
  switch (t) {
    case pack_type::CALL_PACK:
      os << "call_pack";
      break;
    case pack_type::DMA_PACK:
      os << "dma_pack";
      break;
    case pack_type::MMIO_PACK:
      os << "mmio_pack";
      break;
    case pack_type::ETH_PACK:
      os << "eth_pack";
      break;
    case pack_type::SE_PACK:
      os << "single_event_pack";
      break;
    default:
      break;
  }
  return os;
}

struct event_pack {
  uint64_t id_;
  pack_type type_;
  // Stacks stack_; // TODO: not whole stacks is within a stack, but each event
  // on its own
  std::vector<event_t> events_;

  pack_t triggered_by_ = nullptr;
  std::vector<pack_t> triggered_;

  virtual void display(std::ostream &out, unsigned ident) {
    write_ident(out, ident);
    out << "id: " << (unsigned long long)id_ << ", kind: " << type_
        << std::endl;
    write_ident(out, ident);
    out << "was triggered? " << (triggered_by_ != nullptr) << std::endl;
    write_ident(out, ident);
    out << "triggered packs? ";
    for (auto &p : triggered_) {
      out << (unsigned long long)p->id_ << ", ";
    }
    out << std::endl;
    for (event_t event : events_) {
      write_ident(out, ident);
      out << *event << std::endl;
    }
  }

  inline virtual void display(std::ostream &out) {
    display(out, 0);
  }

  virtual bool is_pending() {
    return false;
  }

  virtual bool add_if_triggered(pack_t pack_ptr) {
    return false;
  }

  virtual bool add_on_match(event_t event_ptr) {
    return false;
  }

  bool set_triggered_by(pack_t trigger) {
    if (not triggered_by_) {
      triggered_by_ = trigger;
      return true;
    }
    return false;
  }

  virtual ~event_pack() = default;

 protected:
  event_pack(pack_type t) : id_(get_pack_id()), type_(t) {
  }

  void add_to_pack(event_t event_ptr) {
    if (event_ptr) {
      events_.push_back(event_ptr);
    }
  }

  void add_triggered(pack_t pack_ptr) {
    if (pack_ptr) {
      triggered_.push_back(pack_ptr);
    }
  }

  bool ends_with_offset(uint64_t addr, uint64_t off) {
    size_t lz = std::__countl_zero(off);
    uint64_t mask = lz == 64 ? 0xffffffffffffffff : (1 << (64 - lz)) - 1;
    uint64_t check = addr & mask;
    return check == off;
  }
};

struct call_pack : public event_pack {
  event_t syscall_entry_ = nullptr;
  event_t syscall_return_ = nullptr;
  bool return_indicated_ = false;
  bool is_pending_ = true;
  bool transmits_ = false;
  bool receives_ = false;

  // the call pack has typically a lot of events and can trigger
  // a multitude of stuff, hence we want to know which event triggered what
  std::unordered_map<event_t, pack_t> triggered_;
  // we do not always know how many events an event might trigger,
  // hence we keep a list of potential triggers
  std::vector<std::pair<event_t, pack_type>> potential_trigggers_;

  static bool is_call_pack_related(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }
    return is_type(event_ptr, EventType::HostCall_t);
  }

  static bool is_transmit_call(event_t event_ptr) {
    if (not event_ptr or not is_type(event_ptr, EventType::HostCall_t)) {
      return false;
    }

    auto call = std::static_pointer_cast<HostCall>(event_ptr);
    return call->func_.compare("i40e_maybe_stop_tx") == 0;
  }

  static bool is_receive_call(event_t event_ptr) {
    if (not event_ptr or not is_type(event_ptr, EventType::HostCall_t)) {
      return false;
    }

    static const std::set<std::string> receive_indicator{"tcp_v4_rcv",
                                                         "i40e_napi_poll"};
    auto call = std::static_pointer_cast<HostCall>(event_ptr);
    return receive_indicator.contains(call->func_);
  }

  static bool is_pci_msix_desc_addr(event_t event_ptr) {
    if (not event_ptr or not is_type(event_ptr, EventType::HostCall_t)) {
      return false;
    }

    auto call = std::static_pointer_cast<HostCall>(event_ptr);
    return call->func_.compare("pci_msix_desc_addr") == 0;
  }

  bool is_pending() override {
    return is_pending_;
  }

  bool add_if_triggered(pack_t pack_ptr) override {
    if (not pack_ptr or pack_ptr.get() == this) {
      return false;
    }

    for (auto it = potential_trigggers_.crbegin();
         it != potential_trigggers_.crend(); it++) {
      auto &e_pt_pair = *it;
      if (e_pt_pair.second == pack_ptr->type_) {
        add_triggered(pack_ptr);
        return true;
      }
    }

    return false;
  }

  bool is_transmitting() {
    return transmits_;
  }

  bool is_receiving() {
    return receives_;
  }

  call_pack() : event_pack(pack_type::CALL_PACK) {
  }

  ~call_pack() = default;

  bool add_on_match(event_t event_ptr) override {
    if (not event_ptr) {
      return false;
    }

    /* TODO: add potential triggers!!!!!!!!!!!!! */

    if (not is_type(event_ptr, EventType::HostCall_t)) {
      return false;
    }

    static const std::set<std::string> return_indicator{
        "syscall_return_via_sysret",
        "switch_fpu_return",
        "native_irq_return_iret",
        "fpregs_assert_state_consistent",
        "prepare_exit_to_usermode",
        "syscall_return_slowpath",
        "native_iret",
        "atomic_try_cmpxchg"};
    auto host_call = std::static_pointer_cast<HostCall>(event_ptr);
    if (0 == host_call->func_.compare("entry_SYSCALL_64")) {
      if (syscall_entry_) {
        if (syscall_return_ and is_pending_ and return_indicated_) {
          return false;
        }

        event_t ret = events_.back();
        syscall_return_ = ret;
        is_pending_ = false;

        return false;
      }
      syscall_entry_ = event_ptr;

    } else if (return_indicator.contains(host_call->func_)) {
      if (not syscall_entry_ or not is_pending_) {
        return false;
      }

      return_indicated_ = true;
    }

    transmits_ = transmits_ || call_pack::is_transmit_call(event_ptr);
    receives_ = receives_ || call_pack::is_receive_call(event_ptr);

    if (host_call->func_.compare("i40e_lan_xmit_frame") == 0) {
      potential_trigggers_.push_back(
          std::make_pair(host_call, pack_type::MMIO_PACK));
    }

    add_to_pack(event_ptr);
    return true;
  }
};
struct dma_pack : public event_pack {
  // NicDmaI_t
  event_t dma_issue_ = nullptr;
  // NicDmaEx_t
  event_t nic_dma_execution_ = nullptr;
  // HostDmaW_t or HostDmaR_t
  event_t host_dma_execution_ = nullptr;
  bool is_read_ = true;
  // HostDmaC_t
  event_t host_dma_completion_ = nullptr;
  // NicDmaCW_t or NicDmaCR_t
  event_t nic_dma_completion_ = nullptr;
  bool is_pending_ = true;

  static bool is_dma_pack_related(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }
    const static std::set<EventType> lt{
        EventType::NicDmaI_t,  EventType::NicDmaEx_t, EventType::HostDmaW_t,
        EventType::HostDmaR_t, EventType::HostDmaC_t, EventType::NicDmaCW_t,
        EventType::NicDmaCR_t};
    return lt.contains(event_ptr->getType());
  }

  bool is_read() {
    return is_read_;
  }

  bool is_pending() override {
    return is_pending_;
  }

  dma_pack() : event_pack(pack_type::DMA_PACK) {
  }

  ~dma_pack() = default;

  bool add_on_match(event_t event_ptr) override {
    if (not event_ptr) {
      return false;
    }

    switch (event_ptr->getType()) {
      case EventType::NicDmaI_t: {
        if (dma_issue_) {
          return false;
        }
        dma_issue_ = event_ptr;
        break;
      }

      case EventType::NicDmaEx_t: {
        if (not dma_issue_) {
          return false;
        }
        auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
        auto exec = std::static_pointer_cast<NicDmaEx>(event_ptr);
        if (issue->id_ != exec->id_ or issue->addr_ != exec->addr_) {
          return false;
        }
        nic_dma_execution_ = event_ptr;
        break;
      }

      case EventType::HostDmaW_t:
      case EventType::HostDmaR_t: {
        if (not dma_issue_ /*or not nic_dma_execution_*/) {
          return false;
        }
        auto n_issue = std::static_pointer_cast<NicDmaI>(dma_issue_);

        is_read_ = is_type(event_ptr, EventType::HostDmaR_t);
        auto h_exec = std::static_pointer_cast<HostAddrSizeOp>(event_ptr);
        if (n_issue->addr_ != h_exec->addr_) {
          return false;
        }
        host_dma_execution_ = event_ptr;
        break;
      }

      case EventType::HostDmaC_t: {
        if (not dma_issue_ /*or not nic_dma_execution_*/ or
            not host_dma_execution_) {
          return false;
        }

        auto exec =
            std::static_pointer_cast<HostAddrSizeOp>(host_dma_execution_);
        auto comp = std::static_pointer_cast<HostDmaC>(event_ptr);
        if (exec->id_ != comp->id_) {
          return false;
        }

        host_dma_completion_ = event_ptr;
        break;
      }

      case EventType::NicDmaCW_t:
      case EventType::NicDmaCR_t: {
        if (not dma_issue_ /*or not nic_dma_execution_*/ or
            not host_dma_execution_ or not host_dma_completion_) {
          return false;
        }

        auto issue = std::static_pointer_cast<NicDmaI>(dma_issue_);
        auto comp = std::static_pointer_cast<NicDma>(event_ptr);
        if (issue->id_ != comp->id_ or issue->addr_ != comp->addr_) {
          return false;
        }

        nic_dma_completion_ = event_ptr;
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

struct eth_pack : public event_pack {
  // NicTx or NicRx
  event_t tx_rx_ = nullptr;
  bool is_send_ = false;
  bool is_pending_ = true;

  eth_pack() : event_pack(pack_type::ETH_PACK) {
  }

  ~eth_pack() = default;

  static bool is_eth_pack_related(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    return is_type(event_ptr, EventType::NicTx_t) or
           is_type(event_ptr, EventType::NicRx_t);
  }

  bool is_pending() override {
    return is_pending_;
  }

  bool add_if_triggered(pack_t pack_ptr) override {
    if (not pack_ptr or pack_ptr.get() == this) {
      return false;
    }

    if (not tx_rx_ or not is_send_) {
      return false;
    }

    if (pack_ptr->type_ != pack_type::ETH_PACK) {
      return false;
    }

    std::shared_ptr<eth_pack> np = std::static_pointer_cast<eth_pack>(pack_ptr);
    if (not np->tx_rx_ or np->is_send_) {
      return false;
    }

    add_triggered(pack_ptr);
    return true;
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

struct mmio_pack : public event_pack {
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
  bool is_pending_ = true;

  explicit mmio_pack(bool pci_msix_desc_addr_before)
      : event_pack(pack_type::MMIO_PACK),
        pci_msix_desc_addr_before_(pci_msix_desc_addr_before) {
  }

  ~mmio_pack() = default;

  static bool is_mmio_pack_related(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }
    const static std::set<EventType> lt{
        EventType::HostMmioW_t,         EventType::HostMmioR_t,
        EventType::HostMmioImRespPoW_t, EventType::NicMmioW_t,
        EventType::NicMmioR_t,          EventType::HostMmioCW_t,
        EventType::HostMmioCR_t,
    };
    return lt.contains(event_ptr->getType());
  }

  bool is_read() {
    return is_read_;
  }

  bool is_pending() override {
    return is_pending_;
  }

  bool add_if_triggered(pack_t pack_ptr) override {
    if (not pack_ptr or pack_ptr.get() == this) {
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
struct single_event_pack : public event_pack {
  event_t event_p_ = nullptr;

  single_event_pack() : event_pack(pack_type::SE_PACK) {
  }

  virtual bool is_pending() {
    return event_p_ == nullptr;
  }

  virtual bool add_on_match(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    if (event_p_) {
      return false;
    }

    event_p_ = event_ptr;
    add_to_pack(event_ptr);
    return true;
  }
};

// NOTE: currently analyzing a whole topology is not supported. only the
// analysis of a
//       nic/host pair is supported at the moment
// --> when extending: make sure that events in a pack belong to same source!!!
struct trace {
  bool has_events_ = false;
  std::list<pack_t> finished_packs_;  // TODO: print a trace in order

  std::list<callp_t> pending_call_packs_;
  bool found_host_tx_ = false;
  bool found_host_rx_ = false;
  bool last_call_pci_msix_desc_addr_ = false;

  std::list<mmiop_t> pending_mmio_packs_;
  size_t at_least_still_expected_mmio_ = 0;

  std::list<dmap_t> pending_dma_packs_;

  bool found_nic_tx_ = false;
  bool found_nic_rx_ = false;

  /*
      TODO:

      nic driver func=i40e_maybe_stop_tx = transimt data
      -> expect mmio -> expect dma register read from device
      -> expect dma data read from device
      -> expect dma reads till NicTx
      -> expect Nic dma write to host to indicate data was send

      NicMsix event, we have an interrupt from nic in host
      -> expect host to handle this -> expect mmio -> these may be multiple mmio
     events
  */

  trace() = default;

  ~trace() = default;

  bool is_trace_pending() {
    return not has_events_ or has_pending_call() or has_pending_mmio() or
           has_pending_dma() or not found_host_tx_ or not found_host_rx_ or
           not found_nic_tx_ or not found_nic_rx_ /*or
at_least_still_expected_mmio_ != 0*/
        ;
  }

  inline bool has_pending_call() {
    return not pending_call_packs_.empty();
  }

  inline bool has_pending_mmio() {
    return not pending_mmio_packs_.empty();
  }

  inline bool has_pending_dma() {
    return not pending_dma_packs_.empty();
  }

  bool add_to_trace(event_t event_ptr) {
    if (not event_ptr or not is_trace_pending()) {
      return false;
    }

    bool added = false;
    if (call_pack::is_call_pack_related(event_ptr)) {
      added = add_call(event_ptr);

    } else if (mmio_pack::is_mmio_pack_related(event_ptr)) {
      added = add_mmio(event_ptr);

    } else if (dma_pack::is_dma_pack_related(event_ptr)) {
      added = add_dma(event_ptr);

    } else if (eth_pack::is_eth_pack_related(event_ptr)) {
      added = add_eth(event_ptr);

    } else {
      added = add_generic_single(event_ptr);
    }

    has_events_ = has_events_ || added;
    return added;
  }

  void display(std::ostream &out) {
    out << std::endl;
    out << std::endl;
    out << "Event Trace:" << std::endl;
    out << "\tFinished Packs:" << std::endl;
    for (auto pack : finished_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    out << std::endl;
    out << "\tPendingPacks:" << std::endl;
    for (auto pack : pending_call_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    for (auto pack : pending_mmio_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    for (auto pack : pending_dma_packs_) {
      pack->display(out, 2);
      out << std::endl;
    }
    out << std::endl;
    out << std::endl;
  }

 private:
  void add_pack(pack_t pack) {
    if (not pack) {
      return;
    }

    finished_packs_.push_back(pack);
    // std::cout << "add a pack: " << std::endl;
    // pack->display(std::cout, 1);
  }

  template <typename pack_type>
  std::shared_ptr<pack_type> iterate_add_erase(
      std::list<std::shared_ptr<pack_type>> &pending, event_t event_ptr) {
    auto it = pending.begin();
    while (it != pending.end()) {
      auto cur_pack = *it;
      if (not cur_pack->is_pending()) {
        add_pack(cur_pack);
        it = pending.erase(it);
      } else {
        if (cur_pack->add_on_match(event_ptr)) {
          if (not cur_pack->is_pending()) {
            add_pack(cur_pack);
            pending.erase(it);
          }
          return cur_pack;
        }
        it++;
      }
    }
    return nullptr;
  }

  template <typename pt>
  std::shared_ptr<pt> create_add(
      std::list<std::shared_ptr<pt>> &pending, event_t event_ptr) {
    auto pending_stack = std::make_shared<pt>();
    if (pending_stack and pending_stack->add_on_match(event_ptr)) {
      pending.push_back(pending_stack);
      return pending_stack;
    }
    return nullptr;
  }

  template <typename pt>
  bool add_set_triggered_pending(std::list<std::shared_ptr<pt>> &pending,
                         pack_t pack_ptr) {
    if (not pack_ptr or pack_ptr->is_pending()) {
      return false;
    }

    for (std::shared_ptr<pt> pack : pending) {
      if (pack->add_if_triggered(pack_ptr)) {
        pack_ptr->set_triggered_by(pack);
        return true;
      }
    }

    return false;
  }

  template <typename pt>
  bool add_set_triggered_trace(pack_type type, pack_t pack_ptr) {
    if (not pack_ptr or pack_ptr->is_pending()) {
      return false;
    }

    for (auto it = finished_packs_.rbegin(); it != finished_packs_.rend(); it++) {
      pack_t pack = *it;
      if (pack->type_ != type) {
        continue;
      }

      auto casted_pack = std::static_pointer_cast<pt>(pack);
      if (casted_pack->add_if_triggered(pack_ptr)) {
        pack_ptr->set_triggered_by(casted_pack);
        return true;
      }
    }

    return false;
  }

  bool add_call(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<call_pack> pack = nullptr;
    if (has_pending_call()) {
      pack = iterate_add_erase<call_pack>(pending_call_packs_, event_ptr);
    }
    if (not pack and not(found_nic_rx_ and found_nic_tx_ and found_host_rx_ and
                         found_host_tx_)) {
      pack = create_add<call_pack>(pending_call_packs_, event_ptr);
    }

    if (pack) {
      if (pack->is_transmitting()) {
        found_host_tx_ = true;
      }

      if (pack->is_receiving()) {
        found_host_rx_ = true;
      }
      // when receiving, was it triggered by an hardware interrupt
      // in case yes, this call stack was triggered by this interrupt

      // this capp pack is transmitting, hence we must trigger some sort of
      // mmio or there like, which will eventually cause the nic to send
      // packets, thus we neeed to check if this event is a trigger and create
      // a new pending trigger in that case

      // remove mmio packs after pci_msix_desc_addr
      if (not call_pack::is_pci_msix_desc_addr(event_ptr)) {
        if (last_call_pci_msix_desc_addr_) {
          auto it = pending_mmio_packs_.begin();
          while (it != pending_mmio_packs_.end()) {
            mmiop_t pack = *it;
            if (not pack->is_pending()) {
              add_pack(pack);
              it = pending_mmio_packs_.erase(it);
            } else if (pack->is_pending() and
                       pack->pci_msix_desc_addr_before_ and
                       pack->host_mmio_issue_ and pack->im_mmio_resp_) {
              add_pack(pack);
              it = pending_mmio_packs_.erase(it);
            } else {
              ++it;
            }
          }
        }
        last_call_pci_msix_desc_addr_ = false;
      } else {
        last_call_pci_msix_desc_addr_ = true;
      }
      return true;
    }
    return false;
  }

  bool add_mmio(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<mmio_pack> pack = nullptr;
    if (has_pending_mmio()) {
      pack = iterate_add_erase<mmio_pack>(pending_mmio_packs_, event_ptr);
      // if (pack) {
      // if (not pack->is_pending()) {
      //--at_least_still_expected_mmio_; // TODO: expect dma? --> this is
      //  a
      //  hard and not general true case
      //}
      //  return true;
      //}

      // TODO: as the mmio pack must have been triggered somewhere else,
      // look for yet pending triggers that caused this event (host call or
      // device "interrupt"?)
      if (pack and not pack->is_read_) {
        // TODO: only if is not pending anymore
        add_set_triggered_pending<call_pack>(pending_call_packs_, pack);
      }
    }

    // found everything of this trace, hence do not create a new event
    // as it must have been issued by another trace
    if (found_nic_rx_ and found_nic_tx_ and found_host_rx_ and
        found_host_tx_ and not has_pending_call()) {
      return false;
    }

    if (not pack) {
      pack = std::make_shared<mmio_pack>(last_call_pci_msix_desc_addr_);
      if (not pack or not pack->add_on_match(event_ptr)) {
        return false;
      }
      pending_mmio_packs_.push_back(pack);
    }

    return true;
  }

  bool add_dma(event_t event_ptr) {
    if (not event_ptr) {
      return false;
    }

    std::shared_ptr<dma_pack> pack = nullptr;
    if (has_pending_dma()) {
      pack = iterate_add_erase<dma_pack>(pending_dma_packs_, event_ptr);
      // TODO: only if is not pending anymore
      if (pack) {
        add_set_triggered_trace<mmio_pack>(pack_type::MMIO_PACK, pack);
      } 
    }

    // found everything of this trace, hence do not create a new event
    // as it must have been issued by another trace
    if (found_nic_rx_ and found_nic_tx_ and found_host_rx_ and
        found_host_tx_ and not has_pending_call()) {
      return false;
    }

    if (not pack) {
      pack = create_add<dma_pack>(pending_dma_packs_, event_ptr);
      if (nullptr == pack) {
        return false;
      }
    }

    return true;
  }

  bool add_eth(event_t event_ptr) {
    if (not event_ptr or (found_nic_rx_ and found_nic_tx_ and found_host_rx_ and
                          found_host_tx_ and not has_pending_call())) {
      return false;
    }

    auto p = std::make_shared<eth_pack>();
    if (p and p->add_on_match(event_ptr) and not p->is_pending()) {
      add_pack(p);

      if (is_type(event_ptr, EventType::NicTx_t)) {
        found_nic_tx_ = true;  // TODO: expect dma
      } else if (is_type(event_ptr, EventType::NicRx_t)) {
        found_nic_rx_ = true;  // TODO: expect dma
      }

      // TODO: a transmit must have been triggered, hence find trigger
      //       a receive might have been triggered by a transmit but must not be
      // (event though we currently expect this in general)
      if (p->is_send_) {
        add_set_triggered_trace<mmio_pack>(pack_type::MMIO_PACK, p);
      } else {
        add_set_triggered_trace<eth_pack>(pack_type::ETH_PACK, p);
      }

      return true;
    }
    return false;
  }

  bool add_generic_single(event_t event_ptr) {
    if (not event_ptr or (found_nic_rx_ and found_nic_tx_ and found_host_rx_ and
                          found_host_tx_ and not has_pending_call())) {
      return false;
    }

    auto pack = std::make_shared<single_event_pack>();
    if (pack and pack->add_on_match(event_ptr)) {
      add_pack(pack);

      // TODO: any triggers?!?!?!

      return true;
    }
    return false;
  }
};

struct event_stream_tracer : public sim::coroutine::pipe<event_t> {
  trace_t cur_trace_ = nullptr;
  std::list<trace_t> finished_traces_;

  // handling of yet unmatched events
  std::list<event_t> unmatched_events;
  bool has_unmatched() {
    return not unmatched_events.empty();
  }

  bool add_unmatched(event_t event) {
    static std::set<event_t> blacklisted_events;
    if (blacklisted_events.contains(event)) {
      std::stringstream es;
      es << *event;
      DFLOGERR("try to add unmatched yet already blacklisted event: %s",
               es.str().c_str());
      return false;
    }
    unmatched_events.push_back(event);
    return blacklisted_events.insert(event).second;
  }

  event_t get_unmatched() {
    if (unmatched_events.empty()) {
      return nullptr;
    }
    event_t event_ptr = unmatched_events.front();
    unmatched_events.pop_front();
    return event_ptr;
  }

  task_t process(chan_t *src_chan, chan_t *tar_chan) override {
    if (not src_chan or not tar_chan) {
      co_return;
    }
    msg_t msg;
    event_t event_ptr = nullptr;
    // hostc_t host_call = nullptr;
    bool handle_unmatched = false;

    while (true) {
      if (cur_trace_ and not cur_trace_->is_trace_pending()) {
        DLOGIN("found one stack to finish\n");
        // finish up this stack
        handle_unmatched = true;
        finished_traces_.push_back(cur_trace_);
        cur_trace_->display(std::cout);
        cur_trace_ = nullptr;
      }

      event_ptr = nullptr;
      if (handle_unmatched and has_unmatched()) {
        // we are in this case when we are not longer within a trace
        event_ptr = get_unmatched();
      } else {
        msg = co_await src_chan->read();
        if (msg) {
          event_ptr = msg.value();
          handle_unmatched = false;
        } else if (has_unmatched()) {
          handle_unmatched = true;
          continue;
        } else {
          DLOGIN("no events left for processing\n");
          co_return;
        }
      }

      if (!event_ptr) {
        DLOGERR("no event left but one would be expected\n");
        co_return;
      }

      if (!cur_trace_) {
        cur_trace_ = std::make_shared<trace>();
        if (!cur_trace_) {
          DLOGERR("could not allocate new trace\n");
          co_return;
        }
      }

      if (not cur_trace_->add_to_trace(event_ptr)) {
        if (not handle_unmatched) {
          add_unmatched(event_ptr);
        } else {
          DLOGWARN("found unhandled event unable to handle twice\n");
          event_ptr->display(std::cerr);
          std::cerr << std::endl;
        }
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
    out << "event_stream_tracer traces:" << std::endl;
    for (auto trace : tracer.finished_traces_) {
      trace->display(out);
      out << std::endl;
    }
    if (tracer.cur_trace_) {
      out << "yet unfinished trace:" << std::endl;
      tracer.cur_trace_->display(out);
    }
    out << std::endl;
    out << std::endl;
    return out;
  }
};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_OPERATOR_H_
