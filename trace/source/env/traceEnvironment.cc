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

#include "env/traceEnvironment.h"

std::mutex trace_environment::trace_env_mutex_;

string_internalizer trace_environment::internalizer_;

std::set<const std::string *> trace_environment::linux_net_func_indicator_;

std::set<const std::string *> trace_environment::driver_func_indicator_;

std::set<const std::string *> trace_environment::nw_interface_send_;

std::set<const std::string *> trace_environment::nw_interface_receive_;

std::set<EventType> trace_environment::mmio_related_event_t_;

std::set<EventType> trace_environment::dma_related_event_t_;

std::vector<std::shared_ptr<SymsFilter>> trace_environment::symbol_tables_;

const std::string *trace_environment::get_call_func(
    // NOTE: when calling this function the lock must be already held
    std::shared_ptr<Event> event_ptr) {
  if (not event_ptr or not is_type(event_ptr, EventType::HostCall_t)) {
    return nullptr;
  }
  auto call = std::static_pointer_cast<HostCall>(event_ptr);
  const std::string *func = call->func_;
  if (not func) {
    return nullptr;
  }
  return func;
}

void trace_environment::initialize() {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  mmio_related_event_t_.insert(EventType::HostMmioW_t);
  mmio_related_event_t_.insert(EventType::HostMmioR_t);
  mmio_related_event_t_.insert(EventType::HostMmioImRespPoW_t);
  mmio_related_event_t_.insert(EventType::NicMmioW_t);
  mmio_related_event_t_.insert(EventType::NicMmioR_t);
  mmio_related_event_t_.insert(EventType::HostMmioCW_t);
  mmio_related_event_t_.insert(EventType::HostMmioCR_t);

  dma_related_event_t_.insert(EventType::NicDmaI_t);
  dma_related_event_t_.insert(EventType::NicDmaEx_t);
  dma_related_event_t_.insert(EventType::HostDmaW_t);
  dma_related_event_t_.insert(EventType::HostDmaR_t);
  dma_related_event_t_.insert(EventType::HostDmaC_t);
  dma_related_event_t_.insert(EventType::NicDmaCW_t);
  dma_related_event_t_.insert(EventType::NicDmaCR_t);

  linux_net_func_indicator_.insert(internalizer_.internalize("__sys_socket"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__x64_sys_socket"));
  linux_net_func_indicator_.insert(internalizer_.internalize("sock_create"));
  linux_net_func_indicator_.insert(internalizer_.internalize("__sys_bind"));
  linux_net_func_indicator_.insert(internalizer_.internalize("__x64_sys_bind"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__x64_sys_connect"));
  linux_net_func_indicator_.insert(internalizer_.internalize("__sys_connect"));
  linux_net_func_indicator_.insert(internalizer_.internalize("tcp_release_cb"));
  linux_net_func_indicator_.insert(internalizer_.internalize("tcp_init_sock"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_init_xmit_timers"));
  linux_net_func_indicator_.insert(internalizer_.internalize("tcp_v4_connect"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("ip_route_output_key_hash"));
  linux_net_func_indicator_.insert(internalizer_.internalize("tcp_connect"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_fastopen_defer_connect"));
  linux_net_func_indicator_.insert(internalizer_.internalize("ipv4_dst_check"));
  linux_net_func_indicator_.insert(internalizer_.internalize("tcp_sync_mss"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_initialize_rcv_mss"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_write_queue_purge"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_clear_retrans"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_transmit_skb"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__tcp_transmit_skb"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("tcp_v4_send_check"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__tcp_v4_send_check"));
  linux_net_func_indicator_.insert(internalizer_.internalize("ip_queue_xmit"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__ip_queue_xmit"));
  linux_net_func_indicator_.insert(internalizer_.internalize("ip_local_out"));
  linux_net_func_indicator_.insert(internalizer_.internalize("__ip_local_out"));
  linux_net_func_indicator_.insert(internalizer_.internalize("ip_output"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__ip_finish_output"));
  linux_net_func_indicator_.insert(internalizer_.internalize("dev_queue_xmit"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("__dev_queue_xmit"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("skb_network_protocol"));
  linux_net_func_indicator_.insert(internalizer_.internalize("eth_type_vlan"));
  linux_net_func_indicator_.insert(
      internalizer_.internalize("netdev_start_xmit"));

  driver_func_indicator_.insert(
      internalizer_.internalize("i40e_features_check"));
  driver_func_indicator_.insert(
      internalizer_.internalize("i40e_lan_xmit_frame"));
  driver_func_indicator_.insert(
      internalizer_.internalize("i40e_maybe_stop_tx"));
  driver_func_indicator_.insert(internalizer_.internalize("vlan_get_protocol"));
  driver_func_indicator_.insert(
      internalizer_.internalize("dma_map_single_attrs"));
  driver_func_indicator_.insert(
      internalizer_.internalize("dma_map_page_attrs"));
  driver_func_indicator_.insert(
      internalizer_.internalize("i40e_maybe_stop_tx"));

  // TODO: make full list: write, writev, sendto, sendmsg
  nw_interface_send_.insert(internalizer_.internalize("__sys_sendto"));

  // TODO: make full list: read, readv, recvfrom, recvmsg
  nw_interface_receive_.insert(internalizer_.internalize("__sys_recvmsg"));
}

bool trace_environment::add_symbol_table(const std::string component,
                                         const std::string &file_path,
                                         uint64_t address_offset,
                                         FilterType type,
                                         std::set<std::string> symbol_filter) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  static uint64_t next_id = 0;
  auto filter_ptr = SymsFilter::create(++next_id, std::move(component),
                                       file_path, address_offset, type,
                                       std::move(symbol_filter), internalizer_);

  if (not filter_ptr) {
    return false;
  }
  symbol_tables_.push_back(filter_ptr);
  return true;
}

bool trace_environment::add_symbol_table(const std::string identifier,
                                         const std::string &file_path,
                                         uint64_t address_offset,
                                         FilterType type) {
  return add_symbol_table(std::move(identifier), file_path, address_offset,
                          type, {});
}

std::pair<const std::string *, const std::string *>
trace_environment::symtable_filter(uint64_t address) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (symbol_tables_.empty()) {
    return std::make_pair(nullptr, nullptr);
  }

  for (std::shared_ptr<SymsFilter> symt : symbol_tables_) {
    const std::string *symbol = symt->filter(address);
    if (symbol) {
      return std::make_pair(symbol, &symt->get_component());
    }
  }

  return std::make_pair(nullptr, nullptr);
}

bool trace_environment::is_call_pack_related(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }
  return is_type(event_ptr, EventType::HostCall_t);
}

bool trace_environment::is_driver_tx(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.internalize("i40e_lan_xmit_frame");
}

bool trace_environment::is_driver_rx(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.internalize("i40e_napi_poll");
}

bool trace_environment::is_pci_msix_desc_addr(
    std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.internalize("pci_msix_desc_addr");
}

bool trace_environment::is_mmio_pack_related(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }
  return mmio_related_event_t_.contains(event_ptr->get_type());
}

bool trace_environment::is_dma_pack_related(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }
  return dma_related_event_t_.contains(event_ptr->get_type());
}

bool trace_environment::is_eth_pack_related(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }

  return is_type(event_ptr, EventType::NicTx_t) or
         is_type(event_ptr, EventType::NicRx_t);
}

bool trace_environment::is_msix_related(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }

  return is_type(event_ptr, EventType::NicMsix_t) or
         is_type(event_ptr, EventType::HostMsiX_t);
}

bool trace_environment::is_nw_interface_send(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return nw_interface_send_.contains(func);
}

bool trace_environment::is_nw_interface_receive(
    std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return nw_interface_receive_.contains(func);
}

bool trace_environment::is_socket_connect(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.internalize("__sys_connect");
}

bool trace_environment::is_sys_entry(std::shared_ptr<Event> event_ptr) {
  std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.internalize("entry_SYSCALL_64");
}
