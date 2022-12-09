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

#include <string>

#include "trace/events/events.h"
#include "trace/filter/componenttable.h"
#include "trace/filter/symtable.h"
#include "trace/reader/reader.h"

class LogParser {
 protected:
  const std::string &identifier_;
  LineReader &line_reader_;

  bool parse_timestamp(uint64_t &timestamp);

  bool parse_address(uint64_t &address);

 public:
  explicit LogParser(const std::string &identifier, LineReader &line_reader)
      : identifier_(identifier), line_reader_(line_reader){};

  virtual bool parse(const std::string &log_file_path) = 0;
};

class Gem5Parser : public LogParser {
  SymsFilter &symbol_table_;
  ComponentFilter &component_table_;

 protected:
  bool parse_event(uint64_t timestamp, std::string &symbol);

  bool skip_till_address();

 public:
  explicit Gem5Parser(const std::string &identifier, SymsFilter &symbol_table,
                      ComponentFilter &component_table, LineReader &line_reader)
      : LogParser(identifier, line_reader),
        symbol_table_(symbol_table),
        component_table_(component_table) {
  }

  bool parse(const std::string &log_file_path) override;
};

class NicBmParser : public LogParser {
 protected:
  bool parse_off_len_val_comma(uint64_t &off, size_t &len, uint64_t &val);

  bool parse_op_addr_len_pending(uint64_t &op, uint64_t &addr, size_t &len,
                                 size_t &pending, bool with_pending);

  bool parse_mac_address(uint64_t &address);

  bool parse_sync_info(bool &sync_pcie, bool &sync_eth);

 public:
  explicit NicBmParser(const std::string &identifier, LineReader &line_reader)
      : LogParser(identifier, line_reader) {
  }

  bool parse(const std::string &log_file_path) override;
};

#endif  // SIMBRICKS_TRACE_PARSER_H_