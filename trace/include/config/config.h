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

#include <vector>
#include <string>
#include <set>

#include "concurrencpp/runtime/runtime.h"
#include "util/utils.h"
#include "util/exception.h"
#include "yaml-cpp/yaml.h"

#ifndef SIMBRICKS_TRACE_CONFIG_H_
#define SIMBRICKS_TRACE_CONFIG_H_

class TraceEnvConfig {
 public:
  using IndicatorType = std::string;
  using IndicatorContainer = std::set<IndicatorType>;

  explicit TraceEnvConfig() = default;

  ~TraceEnvConfig() = default;

  template<YAML::NodeType::value Type>
  inline static void CheckKeyAndType(const char *key, const YAML::Node &node) {
    throw_on(not node[key],
             "TraceEnvConfig::CheckKeyAndType node doesnt contain specified key",
             source_loc::current());
    throw_on(node[key].Type() != Type,
             "TraceEnvConfig::CheckKeyAndType node at key doesnt have specified type",
             source_loc::current());
  }

  inline static void IterateAddValue(const YAML::Node &node, IndicatorContainer &container_to_fill) {
    for (std::size_t index = 0; index < node.size(); index++) {
      container_to_fill.insert(node[index].as<std::string>());
    }
  }

  inline static void CheckEmptiness(IndicatorContainer &container_to_check) {
    throw_on(container_to_check.empty(),
             "TraceEnvConfig::CheckEmptiness: the container to check is empty",
             source_loc::current());
  }

  inline static TraceEnvConfig CreateFromYaml(const std::string &config_path) {
    TraceEnvConfig trace_config;

    YAML::Node config_root = YAML::LoadFile(config_path);

    CheckKeyAndType<YAML::NodeType::Sequence>(kLinuxNetFuncIndicatorKey, config_root);
    const YAML::Node net_func_ind = config_root[kLinuxNetFuncIndicatorKey];
    IterateAddValue(net_func_ind, trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kDriverFuncIndicatorKey, config_root);
    const YAML::Node driver_func_ind = config_root[kDriverFuncIndicatorKey];
    IterateAddValue(driver_func_ind, trace_config.driver_func_indicator_);
    IterateAddValue(driver_func_ind, trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kErnelTxIndicatorKey, config_root);
    IterateAddValue(config_root[kErnelTxIndicatorKey], trace_config.kernel_tx_indicator_);
    IterateAddValue(config_root[kErnelTxIndicatorKey], trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kErnelRxIndicatorKey, config_root);
    IterateAddValue(config_root[kErnelRxIndicatorKey], trace_config.kernel_rx_indicator_);
    IterateAddValue(config_root[kErnelRxIndicatorKey], trace_config.linux_net_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kPciWriteIndicatorsKey, config_root);
    IterateAddValue(config_root[kPciWriteIndicatorsKey], trace_config.pci_write_indicators_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kDriverTxIndicatorKey, config_root);
    IterateAddValue(config_root[kDriverTxIndicatorKey], trace_config.driver_tx_indicator_);
    IterateAddValue(config_root[kDriverTxIndicatorKey], trace_config.linux_net_func_indicator_);
    IterateAddValue(config_root[kDriverTxIndicatorKey], trace_config.driver_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kDriverRxIndicatorKey, config_root);
    IterateAddValue(config_root[kDriverRxIndicatorKey], trace_config.driver_rx_indicator_);
    IterateAddValue(config_root[kDriverRxIndicatorKey], trace_config.linux_net_func_indicator_);
    IterateAddValue(config_root[kDriverRxIndicatorKey], trace_config.driver_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Sequence>(kSysEntryKey, config_root);
    IterateAddValue(config_root[kSysEntryKey], trace_config.sys_entry_);

    CheckEmptiness(trace_config.driver_tx_indicator_);
    CheckEmptiness(trace_config.sys_entry_);
    CheckEmptiness(trace_config.linux_net_func_indicator_);
    CheckEmptiness(trace_config.driver_func_indicator_);

    CheckKeyAndType<YAML::NodeType::Scalar>(kMaxBackgroundThreadsKey, config_root);
    trace_config.max_background_threads_ = config_root[kMaxBackgroundThreadsKey].as<size_t>();
    throw_on(trace_config.max_background_threads_ == 0,
             "TraceEnvConfig::Create: max_background_threads_ is 0",
             source_loc::current());

    CheckKeyAndType<YAML::NodeType::Scalar>(kMaxCpuThreadsKey, config_root);
    trace_config.max_cpu_threads_ = config_root[kMaxCpuThreadsKey].as<size_t>();
    throw_on(trace_config.max_cpu_threads_ == 0,
             "TraceEnvConfig::Create: max_cpu_threads_ is 0",
             source_loc::current());

    return trace_config;
  }

  inline IndicatorContainer::const_iterator BeginFuncIndicator() const {
    return linux_net_func_indicator_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndFuncIndicator() const {
    return linux_net_func_indicator_.cend();
  }

  inline IndicatorContainer::const_iterator BeginDriverFunc() const {
    return driver_func_indicator_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndDriverFunc() const {
    return driver_func_indicator_.cend();
  }

  inline IndicatorContainer::const_iterator BeginKernelTx() const {
    return kernel_tx_indicator_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndKernelTx() const {
    return kernel_tx_indicator_.cend();
  }

  inline IndicatorContainer::const_iterator BeginKernelRx() const {
    return kernel_rx_indicator_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndKernelRx() const {
    return kernel_rx_indicator_.cend();
  }

  inline IndicatorContainer::const_iterator BeginPciWrite() const {
    return pci_write_indicators_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndPciWrite() const {
    return pci_write_indicators_.cend();
  }

  inline IndicatorContainer::const_iterator BeginDriverTx() const {
    return driver_tx_indicator_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndDriverTx() const {
    return driver_tx_indicator_.cend();
  }

  inline IndicatorContainer::const_iterator BeginDriverRx() const {
    return driver_rx_indicator_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndDriverRx() const {
    return driver_rx_indicator_.cend();
  }

  inline IndicatorContainer::const_iterator BeginSysEntry() const {
    return sys_entry_.cbegin();
  }

  inline IndicatorContainer::const_iterator EndSysEntry() const {
    return sys_entry_.cend();
  }

  inline size_t GetMaxBackgroundThreads() const {
    return max_background_threads_;
  }

  inline size_t GetMaxCpuThreads() const {
    return max_cpu_threads_;
  }

  concurrencpp::runtime_options GetRuntimeOptions() const {
    auto concurren_options = concurrencpp::runtime_options();
    concurren_options.max_cpu_threads = GetMaxCpuThreads();
    concurren_options.max_background_threads = GetMaxBackgroundThreads();
    //concurren_options.max_background_executor_waiting_time =
    //    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(600));
    //concurren_options.max_thread_pool_executor_waiting_time =
    //    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(600));
    return concurren_options;
  }

 private:
  constexpr static const char *kMaxBackgroundThreadsKey{"MaxBackgroundThreads"};
  size_t max_background_threads_ = 1;
  constexpr static const char *kMaxCpuThreadsKey{"MaxCpuThreads"};
  size_t max_cpu_threads_ = 1;
  constexpr static const char *kLinuxNetFuncIndicatorKey{"LinuxFuncIndicator"};
  IndicatorContainer linux_net_func_indicator_;
  constexpr static const char *kDriverFuncIndicatorKey{"DriverFuncIndicator"};
  IndicatorContainer driver_func_indicator_;
  constexpr static const char *kErnelTxIndicatorKey{"KernelTxIndicator"};
  IndicatorContainer kernel_tx_indicator_;
  constexpr static const char *kErnelRxIndicatorKey{"KernelRxIndicator"};
  IndicatorContainer kernel_rx_indicator_;
  constexpr static const char *kPciWriteIndicatorsKey{"PciWriteIndicator"};
  IndicatorContainer pci_write_indicators_;
  constexpr static const char *kDriverTxIndicatorKey{"DriverTxIndicator"};
  IndicatorContainer driver_tx_indicator_;
  constexpr static const char *kDriverRxIndicatorKey{"DriverRxIndicator"};
  IndicatorContainer driver_rx_indicator_;
  constexpr static const char *kSysEntryKey{"SysEntryIndicator"};
  IndicatorContainer sys_entry_;
};

#endif // SIMBRICKS_TRACE_CONFIG_H_
