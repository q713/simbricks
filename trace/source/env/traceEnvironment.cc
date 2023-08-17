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

#include <utility>

std::mutex TraceEnvironment::trace_env_mutex_;

StringInternalizer TraceEnvironment::internalizer_;

std::set<const std::string *> TraceEnvironment::linux_net_func_indicator_;

std::set<const std::string *> TraceEnvironment::driver_func_indicator_;

std::set<const std::string *> TraceEnvironment::kernel_tx_indicator_;

std::set<const std::string *> TraceEnvironment::kernel_rx_indicator_;

std::set<const std::string *> TraceEnvironment::pci_write_indicators_;

std::set<const std::string *> TraceEnvironment::driver_tx_indicator_;

std::set<const std::string *> TraceEnvironment::driver_rx_indicator_;

std::set<const std::string *> TraceEnvironment::sys_entry_;

std::set<EventType> TraceEnvironment::mmio_related_event_t_;

std::set<EventType> TraceEnvironment::dma_related_event_t_;

std::vector<std::shared_ptr<SymsFilter>> TraceEnvironment::symbol_tables_;

const std::string *TraceEnvironment::get_call_func(
    // NOTE: when calling this function the lock must be already held
    std::shared_ptr<Event> event_ptr) {
  if (not event_ptr or not IsType(event_ptr, EventType::kHostCallT)) {
    return nullptr;
  }
  auto call = std::static_pointer_cast<HostCall>(event_ptr);
  const std::string *func = call->GetFunc();
  if (not func) {
    return nullptr;
  }
  return func;
}

void TraceEnvironment::initialize() {
  //std::lock_guard<std::mutex> lock(trace_env_mutex_);
  mmio_related_event_t_.insert(EventType::kHostMmioWT);
  mmio_related_event_t_.insert(EventType::kHostMmioRT);
  mmio_related_event_t_.insert(EventType::kHostMmioImRespPoWT);
  mmio_related_event_t_.insert(EventType::kNicMmioWT);
  mmio_related_event_t_.insert(EventType::kNicMmioRT);
  mmio_related_event_t_.insert(EventType::kHostMmioCWT);
  mmio_related_event_t_.insert(EventType::kHostMmioCRT);

  dma_related_event_t_.insert(EventType::kNicDmaIT);
  dma_related_event_t_.insert(EventType::kNicDmaExT);
  dma_related_event_t_.insert(EventType::kHostDmaWT);
  dma_related_event_t_.insert(EventType::kHostDmaRT);
  dma_related_event_t_.insert(EventType::kHostDmaCT);
  dma_related_event_t_.insert(EventType::kNicDmaCWT);
  dma_related_event_t_.insert(EventType::kNicDmaCRT);

  linux_net_func_indicator_.insert(internalizer_.Internalize("__sys_socket"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__x64_sys_socket"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("sock_create"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("__sys_bind"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("__x64_sys_bind"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__x64_sys_connect"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("__sys_connect"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("tcp_release_cb"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("tcp_init_sock"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_init_xmit_timers"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("tcp_v4_connect"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("ip_route_output_key_hash"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("tcp_connect"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_fastopen_defer_connect"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("ipv4_dst_check"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("tcp_sync_mss"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_initialize_rcv_mss"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_write_queue_purge"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_clear_retrans"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_transmit_skb"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__tcp_transmit_skb"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("tcp_v4_send_check"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__tcp_v4_send_check"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("ip_queue_xmit"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__ip_queue_xmit"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("ip_local_out"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("__ip_local_out"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("ip_output"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__ip_finish_output"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("dev_queue_xmit"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("__dev_queue_xmit"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("skb_network_protocol"));
  linux_net_func_indicator_.insert(internalizer_.Internalize("eth_type_vlan"));
  linux_net_func_indicator_.insert(
      internalizer_.Internalize("netdev_start_xmit"));

  driver_func_indicator_.insert(
      internalizer_.Internalize("i40e_features_check"));
  driver_func_indicator_.insert(
      internalizer_.Internalize("i40e_lan_xmit_frame"));
  driver_func_indicator_.insert(
      internalizer_.Internalize("i40e_maybe_stop_tx"));
  driver_func_indicator_.insert(internalizer_.Internalize("vlan_get_protocol"));
  driver_func_indicator_.insert(
      internalizer_.Internalize("dma_map_single_attrs"));
  driver_func_indicator_.insert(
      internalizer_.Internalize("dma_map_page_attrs"));
  driver_func_indicator_.insert(
      internalizer_.Internalize("i40e_maybe_stop_tx"));

  // TODO: make sure these are correct
  kernel_tx_indicator_.insert(internalizer_.Internalize("__sys_sendto"));
  kernel_tx_indicator_.insert(internalizer_.Internalize("__sys_sendmsg"));
  kernel_tx_indicator_.insert(internalizer_.Internalize("__sys_sendto"));
  kernel_tx_indicator_.insert(internalizer_.Internalize("__sys_sendto"));
  kernel_tx_indicator_.insert(internalizer_.Internalize("dev_hard_start_xmit"));
  kernel_tx_indicator_.insert(internalizer_.Internalize("netdev_start_xmit"));
  kernel_tx_indicator_.insert(internalizer_.Internalize("dev_queue_xmit"));

  // TODO: make sure these are correct
  //kernel_rx_indicator_.insert(internalizer_.Internalize("recvfrom"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("__sys_recvmsg"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("___sys_recvmsg"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("__x64_sys_recvmsg"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("ip_recv"));
  kernel_rx_indicator_.insert(internalizer_.Internalize("ip_list_rcv"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("netif_receive_skb"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("netif_rx"));
  //kernel_rx_indicator_.insert(internalizer_.Internalize("net_rx_action"));

  pci_write_indicators_.insert(internalizer_.Internalize("pci_msix_write_vector_ctrl"));
  pci_write_indicators_.insert(internalizer_.Internalize("__pci_write_msi_msg"));

  driver_tx_indicator_.insert(internalizer_.Internalize("i40e_lan_xmit_frame"));

  driver_rx_indicator_.insert(internalizer_.Internalize("i40e_napi_poll"));
  driver_rx_indicator_.insert(internalizer_.Internalize("i40e_finalize_xdp_rx"));

  sys_entry_.insert(internalizer_.Internalize("entry_SYSCALL_64"));
  sys_entry_.insert(internalizer_.Internalize("error_entry"));
}

bool TraceEnvironment::add_symbol_table(const std::string component,
                                        std::shared_ptr<concurrencpp::thread_pool_executor> background_executor,
                                        std::shared_ptr<concurrencpp::thread_pool_executor> foreground_executor,
                                        const std::string &file_path,
                                        uint64_t address_offset,
                                        FilterType type,
                                        std::set<std::string> symbol_filter) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  static uint64_t next_id = 0;
  auto filter_ptr =
      SymsFilter::Create(++next_id, component, std::move(background_executor), std::move(foreground_executor),
                         file_path, address_offset, type,
                         std::move(symbol_filter), internalizer_);

  if (not filter_ptr) {
    return false;
  }
  symbol_tables_.push_back(filter_ptr);
  return true;
}

bool TraceEnvironment::add_symbol_table(const std::string identifier,
                                        std::shared_ptr<concurrencpp::thread_pool_executor> background_executor,
                                        std::shared_ptr<concurrencpp::thread_pool_executor> foreground_executor,
                                        const std::string &file_path,
                                        uint64_t address_offset,
                                        FilterType type) {
  return add_symbol_table(identifier,
                          std::move(background_executor),
                          std::move(foreground_executor),
                          file_path,
                          address_offset,
                          type,
                          {});
}

std::pair<const std::string *, const std::string *>
TraceEnvironment::symtable_filter(uint64_t address) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (symbol_tables_.empty()) {
    return std::make_pair(nullptr, nullptr);
  }

  for (const std::shared_ptr<SymsFilter> &symt : symbol_tables_) {
    const std::string *symbol = symt->Filter(address);
    if (symbol) {
      return std::make_pair(symbol, &symt->GetComponent());
    }
  }

  return std::make_pair(nullptr, nullptr);
}

bool TraceEnvironment::is_call_pack_related(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }
  return IsType(event_ptr, EventType::kHostCallT);
}

bool TraceEnvironment::IsDriverTx(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return driver_tx_indicator_.contains(func);
}

bool TraceEnvironment::IsDriverRx(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return driver_rx_indicator_.contains(func);
}

bool TraceEnvironment::is_pci_msix_desc_addr(
    std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.Internalize("pci_msix_desc_addr");
}

bool TraceEnvironment::is_pci_write(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return pci_write_indicators_.contains(func);
}

bool TraceEnvironment::is_mmio_pack_related(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }
  return mmio_related_event_t_.contains(event_ptr->GetType());
}

bool TraceEnvironment::is_dma_pack_related(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }
  return dma_related_event_t_.contains(event_ptr->GetType());
}

bool TraceEnvironment::is_eth_pack_related(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }

  return IsType(event_ptr, EventType::kNicTxT) or
      IsType(event_ptr, EventType::kNicRxT);
}

bool TraceEnvironment::is_msix_related(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  if (not event_ptr) {
    return false;
  }

  return IsType(event_ptr, EventType::kNicMsixT) or
      IsType(event_ptr, EventType::kHostMsiXT);
}

bool TraceEnvironment::IsKernelTx(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_tx_indicator_.contains(func);
}

bool TraceEnvironment::IsKernelRx(
    std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_rx_indicator_.contains(func);
}

bool TraceEnvironment::IsKernelOrDriverTx(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_tx_indicator_.contains(func) or driver_tx_indicator_.contains(func);
}

bool TraceEnvironment::IsKernelOrDriverRx(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_rx_indicator_.contains(func) or driver_rx_indicator_.contains(func);
}

bool TraceEnvironment::is_socket_connect(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.Internalize("__sys_connect");
}

bool TraceEnvironment::IsSysEntry(std::shared_ptr<Event> event_ptr) {
  const std::lock_guard<std::mutex> lock(trace_env_mutex_);
  const std::string *func = get_call_func(event_ptr);
  if (not func) {
    return false;
  }
  return sys_entry_.contains(func);
}

bool TraceEnvironment::IsMsixNotToDeviceBarNumber(int bar) {
  // 1, 2, 3, 4, 5 are currently expected to not end up within the device
  return bar != 0;
}

bool TraceEnvironment::IsToDeviceBarNumber(int bar) {
  // only 0 is currently expected to end up in the device
  return bar == 0;
}
