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

#ifndef SIMBRICKS_TRACE_PARSER_H_
#define SIMBRICKS_TRACE_PARSER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "trace/corobelt/corobelt.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/events.h"
#include "trace/reader/reader.h"
#include "trace/util/componenttable.h"

using event_t = std::shared_ptr<Event>;
using ytask_t = sim::corobelt::yield_task<event_t>;
using sim::trace::env::trace_environment;

class LogParser : public sim::corobelt::producer<event_t> {
 protected:
  const std::string name_;
  const size_t identifier_;
  const std::string log_file_path_;
  LineReader &line_reader_;
  trace_environment &env_;

  bool parse_timestamp(uint64_t &timestamp);

  bool parse_address(uint64_t &address);

  static size_t get_Id() {
    static size_t next_id = 0;
    return next_id++;
  }

 public:
  explicit LogParser(const std::string name, const std::string log_file_path,
                     LineReader &line_reader, trace_environment &env)
      : sim::corobelt::producer<event_t>(),
        name_(std::move(name)),
        identifier_(LogParser::get_Id()),
        log_file_path_(std::move(log_file_path)),
        line_reader_(line_reader),
        env_(env){};

  inline size_t getIdent() {
    return identifier_;
  }

  inline const std::string getName() {
    return name_;
  }

  virtual ytask_t produce() override {
    co_return;
  }
};

class Gem5Parser : public LogParser {
  ComponentFilter &component_table_;

 protected:
  event_t parse_global_event(uint64_t timestamp);

  event_t parse_system_switch_cpus(uint64_t timestamp);

  event_t parse_system_pc_pci_host(uint64_t timestamp);

  event_t parse_system_pc_pci_host_interface(uint64_t timestamp);

  event_t parse_system_pc_simbricks(uint64_t timestamp);

  event_t parse_simbricks_event(uint64_t timestamp);

 public:
  explicit Gem5Parser(const std::string name, const std::string log_file_path,
                      trace_environment &env, ComponentFilter &component_table,
                      LineReader &line_reader)
      : LogParser(std::move(name), std::move(log_file_path), line_reader, env),
        component_table_(component_table) {
  }

  ytask_t produce() override;
};

class NicBmParser : public LogParser {
 protected:
  bool parse_off_len_val_comma(uint64_t &off, size_t &len, uint64_t &val);

  bool parse_op_addr_len_pending(uint64_t &op, uint64_t &addr, size_t &len,
                                 size_t &pending, bool with_pending);

  bool parse_mac_address(uint64_t &address);

  bool parse_sync_info(bool &sync_pcie, bool &sync_eth);

 public:
  explicit NicBmParser(const std::string name, const std::string log_file_path,
                       LineReader &line_reader, trace_environment &env)
      : LogParser(std::move(name), std::move(log_file_path), line_reader, env) {
  }

  ytask_t produce() override;
};

#endif  // SIMBRICKS_TRACE_PARSER_H_