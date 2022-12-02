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

//#define PARSER_DEBUG_ 1

#include "trace/parser/parser.h"

#include <errno.h>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/reader/reader.h"
#include "trace/events/events.h"

#include <inttypes.h>

logparser::timestampopt_t logparser::LogParser::parse_timestamp(
    std::string &line) {
  sim_string_utils::trimL(line);
  logparser::timestamp_t timestamp;
  if (!sim_string_utils::parse_uint_trim(line, 16, &timestamp)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s, could not parse string repr. of timestamp from line '%s'\n",
             identifier_.c_str(), line.c_str());
#endif
    return std::nullopt;
  }
  return timestamp;
}

bool logparser::Gem5Parser::skip_till_address(std::string &line) {
  if (!sim_string_utils::consume_and_trim_till_string(line, "0x")) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse till address in line '%s'\n",
             identifier_.c_str(), line.c_str());
#endif
    return false;
  }
  return true;
}

logparser::addressopt_t logparser::Gem5Parser::parse_address(
    std::string &line) {
  // TODO: act on . plus offset addresses
  logparser::address_t address;
  if (!sim_string_utils::parse_uint_trim(line, 16, &address)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse instr. address from line '%s'\n",
             identifier_.c_str(), line.c_str());
#endif
    return std::nullopt;
  }
  return address;
}

bool logparser::Gem5Parser::parse(const std::string &log_file_path) {
  reader::LineReader line_reader(log_file_path);

  if (!line_reader.is_valid()) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not open file with path '%s'\n", identifier_.c_str(),
             log_file_path.c_str());
#endif
    return false;
  }

  for (std::string line; line_reader.get_next_line(line, true);) {
    logparser::timestampopt_t to = parse_timestamp(line);
    if (!to.has_value()) {
#ifdef PARSER_DEBUG_
      DFLOGWARN("%s: could not parse timestamp from line '%s'\n",
                identifier_.c_str(), line.c_str());
#endif
      continue;
    }
    logparser::timestamp_t t = to.value();

    // TODO: a lot of not mapped addresses... 
    // TODO: parse symbricks events within log file!!!!!!!!!!!
    // TODO: gather more info about executed actions?

    if (!skip_till_address(line)) {
#ifdef PARSER_DEBUG_
      DFLOGWARN(
          "%s: could not skip till an address start (0x) was found in '%s'\n",
          identifier_.c_str(), line.c_str());
#endif
      continue;
    }

    logparser::addressopt_t ao = parse_address(line);
    if (!ao.has_value()) {
#ifdef PARSER_DEBUG_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                identifier_.c_str(), line.c_str());
#endif
      continue;
    }
    logparser::address_t addr = ao.value();

    symtable::filter_ret_t instr_o = symbol_table_.filter(addr);
    if (!instr_o.has_value()) {
#ifdef PARSER_DEBUG_
      DFLOGIN("%s: filter out event at timestamp %u with address %u\n", identifier_.c_str(), t, addr);
#endif
      continue;
    }
    std::string instr = instr_o.value();

    Event event(t, instr);
    std::cout << identifier_ << ": found event --> " << event << std::endl;
  }

  return true;
}

