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
#define PARSER_DEBUG_GEM5_ 1

#include <errno.h>
#include <inttypes.h>

#include <functional>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/events/events.h"
#include "trace/parser/parser.h"

bool Gem5Parser::skip_till_address() {
  if (!line_reader_.consume_and_trim_till_string("0x")) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: could not parse till address in line '%s'\n",
             identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  return true;
}

bool Gem5Parser::parse_event(uint64_t timestamp, std::string &symbol) {
  // NOTE: currently micro ops are ignored
  if (line_reader_.consume_and_trim_char('.')) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: ignore micro op '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  // TODO: parse events from other components?
  // TODO: parse more different events!

  HostCall e(timestamp, symbol);
  std::cout << identifier_ << ": " << e << std::endl;
  return true;
}

bool Gem5Parser::parse(const std::string &log_file_path) {
  if (!line_reader_.open_file(log_file_path)) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return false;
  }

  std::string component;
  uint64_t timestamp, addr;
  while (line_reader_.next_line()) {
    if (!parse_timestamp(timestamp)) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse timestamp from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!line_reader_.consume_and_trim_char(':')) {
      continue;
    }
    line_reader_.trimL();
    component = std::move(line_reader_.extract_and_substr_until(
        sim_string_utils::is_alnum_dot_bar));
    if (component.empty()) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse component from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }
    if (!component_table_.filter(component)) {
      continue;
    }

    if (!skip_till_address()) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN(
          "%s: could not skip till an address start (0x) was found in '%s'\n",
          identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!parse_address(addr)) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    // filter out uninteresting addresses
    std::string symbol;
    if (!symbol_table_.filter(addr, symbol)) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGIN("%s: filter out event at timestamp %u with address %u\n",
              identifier_.c_str(), timestamp, addr);
#endif
      continue;
    }

    // parse instructions
    if (!parse_event(timestamp, symbol)) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGIN("%s: could not parse event in '%s'\n", identifier_.c_str(),
              line_reader_.get_raw_line().c_str());
#endif
    }
  }

  return true;
}