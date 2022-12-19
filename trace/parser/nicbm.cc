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

#define PARSER_DEBUG_NICBM_ 1

#include <errno.h>
#include <inttypes.h>

#include "lib/utils/log.h"
#include "trace/events/events.h"
#include "trace/parser/parser.h"

bool NicBmParser::parse_sync_info(bool &sync_pcie, bool &sync_eth) {
  if (line_reader_.consume_and_trim_till_string("sync_pci")) {
    if (!line_reader_.consume_and_trim_char('=')) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line_reader_.get_cur_string().c_str());
#endif
      return false;
    }

    if (line_reader_.consume_and_trim_char('1')) {
      sync_pcie = true;
    } else if (line_reader_.consume_and_trim_char('0')) {
      sync_pcie = false;
    } else {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (!line_reader_.consume_and_trim_till_string("sync_eth")) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: could not find sync_eth in line '%s'\n",
               identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (!line_reader_.consume_and_trim_char('=')) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (line_reader_.consume_and_trim_char('1')) {
      sync_eth = true;
    } else if (line_reader_.consume_and_trim_char('0')) {
      sync_eth = false;
    } else {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    return true;
  }

  std::cout << "not entered first if" << std::endl;

  return false;
}

bool NicBmParser::parse_mac_address(uint64_t &mac_address) {
  if (line_reader_.consume_and_trim_till_string("mac_addr")) {
    if (!line_reader_.consume_and_trim_char('=')) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: mac_addr line '%s' has wrong format\n", identifier_.c_str(),
               line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (!parse_address(mac_address)) {
      return false;
    }
    return true;
  }
  return false;
}

bool NicBmParser::parse_off_len_val_comma(uint64_t &off, uint64_t &len,
                                          uint64_t &val) {
  // parse off
  if (!line_reader_.consume_and_trim_till_string("off=0x")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address(off)) {
    return false;
  }

  // parse len
  if (!line_reader_.consume_and_trim_till_string("len=") ||
      !line_reader_.parse_uint_trim(10, len)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len= in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  // parse val
  if (!line_reader_.consume_and_trim_till_string("val=0x")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address(val)) {
    return false;
  }

  return true;
}

bool NicBmParser::parse_op_addr_len_pending(uint64_t &op, uint64_t &addr,
                                            uint64_t &len, uint64_t &pending,
                                            bool with_pending) {
  // parse op
  if (!line_reader_.consume_and_trim_till_string("op 0x")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse op 0x in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address(op)) {
    return false;
  }

  // parse addr
  if (!line_reader_.consume_and_trim_till_string("addr ")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse addr in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address(addr)) {
    return false;
  }

  // parse len
  if (!line_reader_.consume_and_trim_till_string("len ") ||
      !line_reader_.parse_uint_trim(10, len)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  if (!with_pending) {
    return true;
  }

  // parse pending
  if (!line_reader_.consume_and_trim_till_string("pending ") ||
      !line_reader_.parse_uint_trim(10, pending)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse pending in line '%s'\n", identifier_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  return true;
}

void NicBmParser::produce(corobelt::coro_push_t<std::shared_ptr<Event>> &sink) {
  if (!line_reader_.open_file(log_file_path_)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return;
  }

  uint64_t mac_address = 0;
  bool sync_pci = false;
  bool sync_eth = false;

  // parse mac address and sync informations
  if (line_reader_.next_line()) {
    if (!parse_mac_address(mac_address)) {
      return;
    }
#ifdef PARSER_DEBUG_NICBM_
    DFLOGIN("%s: found mac_addr=%lx\n", identifier_.c_str(), mac_address);
#endif
  }
  if (line_reader_.next_line()) {
    if (!parse_sync_info(sync_pci, sync_eth)) {
      return;
    }
#ifdef PARSER_DEBUG_NICBM_
    DFLOGIN("%s: found sync_pcie=%d sync_eth=%d\n", identifier_.c_str(),
            sync_pci, sync_eth);
#endif
  }

  uint64_t timestamp, off, val, op, addr, vec, len, pending;
  // parse the actual events of interest
  while (line_reader_.next_line()) {
    line_reader_.trimL();
    if (line_reader_.consume_and_trim_till_string(
            "exit main_time")) {  // end of event loop
                                  // TODO: may parse nic statistics as well
#ifdef PARSER_DEBUG_NICBM_
      DFLOGIN("%s: found exit main_time %s\n", identifier_.c_str(),
              line_reader_.get_raw_line().c_str());
#endif
      continue;
    } else if (line_reader_.consume_and_trim_till_string(
                   "poll_h2d: peer terminated")) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGIN("%s: found poll_h2d: peer terminated\n", identifier_.c_str());
#endif
      continue;
    }

    // main parsing
    if (line_reader_.consume_and_trim_till_string(
            "main_time")) {  // main parsing
      if (!line_reader_.consume_and_trim_string(" = ")) {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: main line '%s' has wrong format\n", identifier_.c_str(),
                 line_reader_.get_raw_line().c_str());
#endif
        continue;
      }

      if (!parse_timestamp(timestamp)) {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: could not parse timestamp in line '%s'",
                 identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }

      if (!line_reader_.consume_and_trim_till_string("nicbm")) {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: line '%s' has wrong format for parsing event info",
                 identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }

      if (line_reader_.consume_and_trim_till_string("read(")) {
        if (!parse_off_len_val_comma(off, len, val)) {
          continue;
        }
        sink(std::make_shared<NicMmioR>(timestamp, off, len, val));

      } else if (line_reader_.consume_and_trim_till_string("write(")) {
        if (!parse_off_len_val_comma(off, len, val)) {
          continue;
        }
        sink(std::make_shared<NicMmioW>(timestamp, off, len, val));

      } else if (line_reader_.consume_and_trim_till_string("issuing dma")) {
        if (!parse_op_addr_len_pending(op, addr, len, pending, true)) {
          continue;
        }
        sink(std::make_shared<NicDmaI>(timestamp, op, addr, len));

      } else if (line_reader_.consume_and_trim_till_string("executing dma")) {
        if (!parse_op_addr_len_pending(op, addr, len, pending, true)) {
          continue;
        }
        sink(std::make_shared<NicDmaE>(timestamp, op, addr, len));

      } else if (line_reader_.consume_and_trim_till_string(
                     "completed dma read")) {
        if (!parse_op_addr_len_pending(op, addr, len, pending, false)) {
          continue;
        }
        sink(std::make_shared<NicDmaCR>(timestamp, op, addr, len));

      } else if (line_reader_.consume_and_trim_till_string(
                     "completed dma write")) {
        if (!parse_op_addr_len_pending(op, addr, len, pending, false)) {
          continue;
        }
        sink(std::make_shared<NicDmaCW>(timestamp, op, addr, len));

      } else if (line_reader_.consume_and_trim_till_string(
                     "issue MSI-X interrupt vec ")) {
        if (!line_reader_.parse_uint_trim(10, vec)) {
          continue;
        }
        sink(std::make_shared<NicMsix>(timestamp, vec));

      } else if (line_reader_.consume_and_trim_till_string("eth tx: len ")) {
        if (!line_reader_.parse_uint_trim(10, len)) {
          continue;
        }
        sink(std::make_shared<NicTx>(timestamp, len));

      } else if (line_reader_.consume_and_trim_till_string(
                     "eth rx: port 0 len ")) {
        if (!line_reader_.parse_uint_trim(10, len)) {
          continue;
        }
        sink(std::make_shared<NicRx>(timestamp, len));
        
      } else {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: line '%s' did not match any expected main line\n",
                 identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }
    } else {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGWARN("%s: could not parse given line '%s'\n", identifier_.c_str(),
                line_reader_.get_raw_line().c_str());
#endif
      continue;
    }
  }

  return;
}