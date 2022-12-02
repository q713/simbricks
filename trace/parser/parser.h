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

#pragma once

#include <optional>
#include <string>

#include "trace/filter/symtable.h"

namespace logparser {

using timestamp_t = uint64_t;
using timestampopt_t = std::optional<timestamp_t>;
using address_t = uint64_t;
using addressopt_t = std::optional<address_t>;

class LogParser {
 protected:
  const std::string &identifier_;
  symtable::SymsFilter &symbol_table_;

 public:
  explicit LogParser(const std::string &identifier,
                     symtable::SymsFilter &symbol_table)
      : identifier_(identifier), symbol_table_(symbol_table){};

  timestampopt_t parse_timestamp(std::string &line);

  virtual bool parse(const std::string &log_file_path) = 0;
};

class Gem5Parser : public LogParser {
 protected:
  bool skip_till_address(std::string &line);

  addressopt_t parse_address(std::string &line);

 public:
  explicit Gem5Parser(const std::string &identifier,
                      symtable::SymsFilter &symbol_table)
      : LogParser(identifier, symbol_table) {
  }

  bool parse(const std::string &log_file_path) override;
};

class NicBmParser : public LogParser {
  public:
  explicit NicBmParser(const std::string &identifier,
                       symtable::SymsFilter &symbol_table)
      : LogParser(identifier, symbol_table) {
  }
};

}  // namespace logparser

#endif  // SIMBRICKS_TRACE_PARSER_H_