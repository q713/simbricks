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

#include <memory>
#include <string>

#include "trace/corobelt/coroutine.h"
#include "trace/events/events.h"
#include "trace/filter/componenttable.h"
#include "trace/filter/symtable.h"
#include "trace/reader/reader.h"
#include "trace/filter/symtable.h"

using event_t = std::shared_ptr<Event>;
using task_t = sim::coroutine::task<void>;
using chan_t = sim::coroutine::unbuffered_single_chan<event_t>;
class LogParser : public sim::coroutine::producer<event_t> {
 protected:
  const std::string identifier_;
  const std::string log_file_path_;
  LineReader &line_reader_;

  bool parse_timestamp(uint64_t &timestamp);

  bool parse_address(uint64_t &address);

 public:
  explicit LogParser(const std::string identifier,
                     const std::string log_file_path, LineReader &line_reader)
      : sim::coroutine::producer<event_t>(),
        identifier_(std::move(identifier)),
        log_file_path_(std::move(log_file_path)),
        line_reader_(line_reader){};

  const std::string &getIdent() {
    return identifier_;
  }

  virtual task_t produce(chan_t *tar_chan) override {
    co_return;
  }
};

class Gem5Parser : public LogParser {
  SymsFilter &symbol_table_;
  ComponentFilter &component_table_;

 protected:
  event_t parse_global_event(uint64_t timestamp);

  event_t parse_system_switch_cpus(uint64_t timestamp);

  event_t parse_system_pc_pci_host(uint64_t timestamp);

  event_t parse_system_pc_pci_host_interface(uint64_t timestamp);

  event_t parse_system_pc_simbricks(uint64_t timestamp);

  event_t parse_simbricks_event(uint64_t timestamp);

 public:
  explicit Gem5Parser(const std::string identifier,
                      const std::string log_file_path, SymsFilter &symbol_table,
                      ComponentFilter &component_table, LineReader &line_reader)
      : LogParser(std::move(identifier), std::move(log_file_path), line_reader),
        symbol_table_(symbol_table),
        component_table_(component_table) {
  }

  task_t produce(chan_t *tar_chan) override;
};

class NicBmParser : public LogParser {
 protected:
  bool parse_off_len_val_comma(uint64_t &off, size_t &len, uint64_t &val);

  bool parse_op_addr_len_pending(uint64_t &op, uint64_t &addr, size_t &len,
                                 size_t &pending, bool with_pending);

  bool parse_mac_address(uint64_t &address);

  bool parse_sync_info(bool &sync_pcie, bool &sync_eth);

 public:
  explicit NicBmParser(const std::string identifier,
                       const std::string log_file_path, LineReader &line_reader)
      : LogParser(std::move(identifier), std::move(log_file_path),
                  line_reader) {
  }

  task_t produce(chan_t *tar_chan) override;
};

#endif  // SIMBRICKS_TRACE_PARSER_H_