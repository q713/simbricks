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

#ifndef SIMBRICKS_TRACE_EVENT_CALL_PACK_H_
#define SIMBRICKS_TRACE_EVENT_CALL_PACK_H_

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "trace/analytics/packs/pack.h"
#include "trace/analytics/packs/mmioPack.h"
#include "trace/analytics/config.h"
#include "trace/events/events.h"

using event_t = std::shared_ptr<Event>;
using pack_t = std::shared_ptr<event_pack>;

struct call_pack : public event_pack {
  event_t call_pack_entry_ = nullptr;
  event_t syscall_return_ = nullptr;
  bool transmits_ = false;
  bool receives_ = false;
  bool is_relevant_ = false;

  std::unordered_map<event_t, pack_t> triggered_;
  std::list<event_t> send_trigger_;
  std::list<event_t> receiver_;

  bool add_if_triggered(pack_t pack_ptr) override {
    if (not potentially_triggered(pack_ptr)) {
      return false;
    }

    if (is_type(pack_ptr, pack_type::MMIO_PACK)) {
      if (send_trigger_.empty()) {
        return false;
      }

      auto mmio_p = std::static_pointer_cast<mmio_pack>(pack_ptr);
      event_t trigger = nullptr;
      auto it = send_trigger_.crbegin();
      for ( ; it != send_trigger_.crend(); it++) {
        event_t p = *it;
        if (p->timestamp_ < mmio_p->host_mmio_issue_->timestamp_) {
          trigger = p;
          break;
        }
      }

      if (not trigger) {
        return false;
      }
      add_triggered(pack_ptr);
      triggered_.insert({trigger, pack_ptr});
      std::advance(it, 1);
      send_trigger_.erase(it.base());
      return true;
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

    if (not is_type(event_ptr, EventType::HostCall_t) or not is_pending_) {
      return false;
    }

    auto host_call = std::static_pointer_cast<HostCall>(event_ptr);
    const std::string &func_name = host_call->func_; 
    if (0 == func_name.compare("entry_SYSCALL_64")) {
      if (call_pack_entry_) {
        is_pending_ = false;
        syscall_return_ = events_.back();
        //std::cout << "found call stack end" << std::endl;
        //this->display(std::cout);
        //std::cout << std::endl;
        return false;
      }
      is_pending_ = true;
      call_pack_entry_ = event_ptr;
      add_to_pack(event_ptr);
      return true;
    } 

    if (not call_pack_entry_) {
      return false;
    }
    
    if (sim::analytics::conf::is_transmit_call(event_ptr)) {
      transmits_ = true;
      send_trigger_.push_back(event_ptr);

    // TODO: where does the kernel actually "receive" the packet
    } else if (sim::analytics::conf::is_receive_call(event_ptr)) {
      receives_ = true;
      receiver_.push_back(event_ptr);
    }

    is_relevant_ = is_relevant_ || transmits_ || receives_;
                  //sim::analytics::conf::LINUX_NET_STACK_FUNC_INDICATOR.contains(
                  //     host_call->func_);

    add_to_pack(event_ptr);
    return true;
  }
};

//static const std::set<std::string> return_indicator{
//        "syscall_return_via_sysret",
//        "switch_fpu_return",
//        "native_irq_return_iret",
//        "fpregs_assert_state_consistent",
//        "prepare_exit_to_usermode",
//        "syscall_return_slowpath",
//        "native_iret",
//        "atomic_try_cmpxchg"};

#endif  // SIMBRICKS_TRACE_EVENT_CALL_PACK_H_