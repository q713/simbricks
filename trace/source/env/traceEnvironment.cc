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

const std::string *TraceEnvironment::GetCallFunc(
    // NOTE: when calling this function the lock must be already held
    std::shared_ptr<Event> &event_ptr) {
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

TraceEnvironment::TraceEnvironment(const TraceEnvConfig &trace_env_config)
    : trace_env_config_(trace_env_config),
      runtime_(trace_env_config_.GetRuntimeOptions()) {
  InternalizeStrings(trace_env_config_.BeginFuncIndicator(),
                     trace_env_config_.EndFuncIndicator(),
                     linux_net_func_indicator_);

  InternalizeStrings(trace_env_config_.BeginDriverFunc(),
                     trace_env_config_.EndDriverFunc(),
                     driver_func_indicator_);

  InternalizeStrings(trace_env_config_.BeginKernelTx(),
                     trace_env_config_.EndKernelTx(),
                     kernel_tx_indicator_);

  InternalizeStrings(trace_env_config_.BeginKernelRx(),
                     trace_env_config_.EndKernelRx(),
                     kernel_rx_indicator_);

  InternalizeStrings(trace_env_config_.BeginPciWrite(),
                     trace_env_config_.EndPciWrite(),
                     pci_write_indicators_);

  InternalizeStrings(trace_env_config_.BeginDriverTx(),
                     trace_env_config_.EndDriverTx(),
                     driver_tx_indicator_);

  InternalizeStrings(trace_env_config_.BeginDriverRx(),
                     trace_env_config_.EndDriverRx(),
                     driver_rx_indicator_);

  InternalizeStrings(trace_env_config_.BeginSysEntry(),
                     trace_env_config_.EndSysEntry(),
                     sys_entry_);
}

bool TraceEnvironment::AddSymbolTable(const std::string identifier,
                                      const std::string &file_path,
                                      uint64_t address_offset,
                                      FilterType type,
                                      std::set<std::string> symbol_filter) {
  const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
  static uint64_t next_id = 0;
  auto filter_ptr =
      SymsFilter::Create(++next_id, identifier,
                         file_path, address_offset, type,
                         std::move(symbol_filter), internalizer_);

  if (not filter_ptr) {
    return false;
  }
  symbol_tables_.push_back(filter_ptr);
  return true;
}

bool TraceEnvironment::AddSymbolTable(const std::string identifier,
                                      const std::string &file_path,
                                      uint64_t address_offset,
                                      FilterType type) {
  return AddSymbolTable(identifier,
                        file_path,
                        address_offset,
                        type,
                        {});
}

std::pair<const std::string *, const std::string *>
TraceEnvironment::SymtableFilter(uint64_t address) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
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

bool TraceEnvironment::IsDriverTx(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return driver_tx_indicator_.contains(func);
}

bool TraceEnvironment::IsDriverRx(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return driver_rx_indicator_.contains(func);
}

bool TraceEnvironment::IsPciMsixDescAddr(
    std::shared_ptr<Event> event_ptr) {
  const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.Internalize("pci_msix_desc_addr");
}

bool TraceEnvironment::is_pci_write(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return pci_write_indicators_.contains(func);
}

bool TraceEnvironment::IsKernelTx(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_tx_indicator_.contains(func);
}

bool TraceEnvironment::IsKernelRx(
    std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_rx_indicator_.contains(func);
}

bool TraceEnvironment::IsKernelOrDriverTx(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_tx_indicator_.contains(func) or driver_tx_indicator_.contains(func);
}

bool TraceEnvironment::IsKernelOrDriverRx(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return kernel_rx_indicator_.contains(func) or driver_rx_indicator_.contains(func);
}

bool TraceEnvironment::IsSocketConnect(std::shared_ptr<Event> event_ptr) {
  const std::unique_lock writer_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
  if (not func) {
    return false;
  }
  return func == internalizer_.Internalize("__sys_connect");
}

bool TraceEnvironment::IsSysEntry(std::shared_ptr<Event> event_ptr) {
  const std::shared_lock reader_lock(trace_env_reader_writer_mutex_);
  const std::string *func = GetCallFunc(event_ptr);
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
