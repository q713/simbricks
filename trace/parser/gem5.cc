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

#include "trace/parser/parser.h"

#define PARSER_DEBUG_GEM5_ 1

#include <errno.h>
#include <inttypes.h>

#include <functional>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/events/events.h"

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

bool Gem5Parser::parse_switch_cpus_event(
    corobelt::coro_push_t<std::shared_ptr<Event>> &sink, uint64_t timestamp) {
  if (!skip_till_address()) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN(
        "%s: could not skip till an address start (0x) was found in '%s'\n",
        identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  uint64_t addr;
  if (!parse_address(addr)) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse address from line '%s'\n",
              identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  // filter out uninteresting addresses
  std::string symbol;
  if (!symbol_table_.filter(addr, symbol)) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGIN("%s: filter out event at timestamp %u with address %u\n",
            identifier_.c_str(), timestamp, addr);
#endif
    return false;
  }

  // NOTE: currently micro ops are ignored
  if (line_reader_.consume_and_trim_char('.')) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: ignore micro op '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  // TODO: parse more about the instruction itself (MicroOps etc.)!

  sink(std::make_shared<HostCall>(timestamp, this, symbol));
  return true;
}

bool Gem5Parser::parse_simbricks_event(
    corobelt::coro_push_t<std::shared_ptr<Event>> &sink, uint64_t timestamp) {
  if (line_reader_.consume_and_trim_char(':')) {
    line_reader_.trimL();
    if (line_reader_.consume_and_trim_string("processInEvent")) {
      sink(std::make_shared<SimProcInEvent>(timestamp, this));
      return true;
    } else if (line_reader_.consume_and_trim_string("sending sync message")) {
      sink(std::make_shared<SimSendSync>(timestamp, this));
      return true;
    }
  } else if (line_reader_.consume_and_trim_string("-pci:")) {
    uint64_t id, addr, size;
    line_reader_.trimL();
    if (line_reader_.consume_and_trim_string("received ")) {
      int isReadOrWrite = 0;
      if (line_reader_.consume_and_trim_string("write ")) {
        isReadOrWrite = -1;
      } else if (line_reader_.consume_and_trim_string("read ")) {
        isReadOrWrite = 1;
      }
      if (isReadOrWrite != 0 &&
          line_reader_.consume_and_trim_string("completion id ") &&
          line_reader_.parse_uint_trim(10, id)) {
        if (isReadOrWrite == -1) {
          sink(std::make_shared<HostMmioCW>(timestamp, this, id));
        } else {
          sink(std::make_shared<HostMmioCR>(timestamp, this, id));
        }
        return true;
      }

    } else if (line_reader_.consume_and_trim_string("sending ")) {
      int isReadWrite = 0;
      if (line_reader_.consume_and_trim_string("read addr ")) {
        isReadWrite = 1;
      } else if (line_reader_.consume_and_trim_string("write addr ")) {
        isReadWrite = -1;
      } else if (line_reader_.consume_and_trim_string(
                     "sending immediate response for posted write")) {
        sink(std::make_shared<HostMmioImRespPoW>(timestamp, this));
        return true;
      }

      if (isReadWrite != 0 && line_reader_.parse_uint_trim(16, addr) &&
          line_reader_.consume_and_trim_string(" size ") &&
          line_reader_.parse_uint_trim(10, size) &&
          line_reader_.consume_and_trim_string(" id ") &&
          line_reader_.parse_uint_trim(10, id)) {
        if (isReadWrite == 1) {
          sink(std::make_shared<HostMmioR>(timestamp, this, id, addr, size));
        } else {
          sink(std::make_shared<HostMmioW>(timestamp, this, id, addr, size));
        }
        return true;
      }
    }
    // sending immediate response for posted write
  }

#ifdef PARSER_DEBUG_GEM5_
  DFLOGERR("%s: found unknown simbricks event in line '%s'\n",
           identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
  return false;
}

void Gem5Parser::produce(corobelt::coro_push_t<std::shared_ptr<Event>> &sink) {
  if (!line_reader_.open_file(log_file_path_)) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return;
  }

  std::string component;
  uint64_t timestamp;
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
    if (component.empty() || !line_reader_.consume_and_trim_char(':')) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse component from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }
    if (!component_table_.filter(component)) {
      continue;
    }

    if (line_reader_.consume_and_trim_till_string("simbricks")) {
      line_reader_.trimL();
      if (!parse_simbricks_event(sink, timestamp)) {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGERR("%s: could not parse simbricks event from line '%s'\n",
                 identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      }
      continue;
    }

    if (component == "system.switch_cpus") {
      if (!parse_switch_cpus_event(sink, timestamp)) {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGIN("%s: could not parse system.switch_cpus event in line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      }
      continue;
    }

#ifdef PARSER_DEBUG_GEM5_
    DFLOGIN("%s: could not parse event in line '%s'\n", identifier_.c_str(),
            line_reader_.get_raw_line().c_str());
#endif
  }

  return;
}