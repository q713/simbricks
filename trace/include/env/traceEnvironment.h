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
#ifndef SIM_TRACE_CONFIG_VARS_H_
#define SIM_TRACE_CONFIG_VARS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "events/events.h"
#include "env/stringInternalizer.h"
#include "env/symtable.h"


// TODO: add proper configuration support for the tracing environment!!!!
class TraceEnvironment {
  static std::mutex trace_env_mutex_;

  static StringInternalizer internalizer_;

  static std::set<const std::string *> linux_net_func_indicator_;

  static std::set<const std::string *> driver_func_indicator_;

  static std::set<const std::string *> kernel_tx_indicator_;

  static std::set<const std::string *> kernel_rx_indicator_;

  static std::set<const std::string *> pci_write_indicators_;

  static std::set<const std::string *> driver_tx_indicator_;

  static std::set<const std::string *> driver_rx_indicator_;

  static std::set<const std::string *> sys_entry_;

  static std::set<EventType> mmio_related_event_t_;

  static std::set<EventType> dma_related_event_t_;

  static std::vector<std::shared_ptr<SymsFilter>> symbol_tables_;

 protected:
  static const std::string *get_call_func(std::shared_ptr<Event> event_ptr);

 public:
  static void initialize();

  inline static StringInternalizer &get_internalizer() {
    return internalizer_;
  }

  inline static std::vector<std::shared_ptr<SymsFilter>> &get_symtables() {
    return symbol_tables_;
  }

  inline static uint64_t GetNextParserId() {
    const std::lock_guard<std::mutex> lock(trace_env_mutex_);
    static uint64_t next_id = 0;
    return next_id++;
  }

  inline static uint64_t GetNextSpanId() {
    const std::lock_guard<std::mutex> lock(trace_env_mutex_);
    static uint64_t next_id = 0;
    return next_id++;
  }

  inline static uint64_t GetNextSpannerId() {
    const std::lock_guard<std::mutex> lock(trace_env_mutex_);
    static uint64_t next_id = 0;
    return next_id++;
  }

  inline static uint64_t GetNextTraceId() {
    const std::lock_guard<std::mutex> lock(trace_env_mutex_);
    static uint64_t next_id = 0;
    return next_id++;
  }

  inline static uint64_t GetNextTraceContextId() {
    const std::lock_guard<std::mutex> lock(trace_env_mutex_);
    static uint64_t next_id = 0;
    return next_id++;
  }

  inline static const std::string *internalize_additional(
      const std::string &symbol) {
    //std::lock_guard<std::mutex> lock(trace_env_mutex_);
    return internalizer_.Internalize(symbol);
  }

  static bool add_symbol_table(const std::string identifier,
                               const std::string &file_path,
                               uint64_t address_offset, FilterType type,
                               std::set<std::string> symbol_filter);

  static bool add_symbol_table(const std::string identifier,
                               const std::string &file_path,
                               uint64_t address_offset, FilterType type);

  static std::pair<const std::string *, const std::string *> symtable_filter(
      uint64_t address);

  static bool is_call_pack_related(std::shared_ptr<Event> event_ptr);

  static bool IsDriverTx(std::shared_ptr<Event> event_ptr);

  static bool IsDriverRx(std::shared_ptr<Event> event_ptr);

  static bool is_pci_msix_desc_addr(std::shared_ptr<Event> event_ptr);

  static bool is_pci_write(std::shared_ptr<Event> event_ptr);

  static bool is_mmio_pack_related(std::shared_ptr<Event> event_ptr);

  static bool is_dma_pack_related(std::shared_ptr<Event> event_ptr);

  static bool is_eth_pack_related(std::shared_ptr<Event> event_ptr);

  static bool is_msix_related(std::shared_ptr<Event> event_ptr);

  static bool IsKernelTx(std::shared_ptr<Event> event_ptr);

  static bool IsKernelRx(std::shared_ptr<Event> event_ptr);

  static bool IsKernelOrDriverTx(std::shared_ptr<Event> event_ptr);

  static bool IsKernelOrDriverRx(std::shared_ptr<Event> event_ptr);

  static bool is_socket_connect(std::shared_ptr<Event> event_ptr);

  static bool IsSysEntry(std::shared_ptr<Event> event_ptr);

  static bool IsMsixNotToDeviceBarNumber(int bar);

  static bool IsToDeviceBarNumber(int bar);
};

#endif  // SIM_TRACE_CONFIG_VARS_H_