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

#define PARSER_DEBUG_ 1

#include "trace/parser/parser.h"

#include <errno.h>
#include <inttypes.h>

#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
#include "trace/events/events.h"

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

logparser::addressopt_t logparser::LogParser::parse_address(std::string &line) {
  // TODO: act on . plus offset addresses
  logparser::address_t address;
  if (!sim_string_utils::parse_uint_trim(line, 16, &address)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse address from line '%s'\n",
             identifier_.c_str(), line.c_str());
#endif
    return std::nullopt;
  }
  return address;
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

bool logparser::Gem5Parser::parse(const std::string &log_file_path) {
  auto line_reader_opt = reader::LineReader::create(log_file_path);
  if (!line_reader_opt.has_value()) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return false;
  }
  auto line_reader = std::move(line_reader_opt.value());

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
      continue;
    }
    logparser::address_t addr = ao.value();

    symtable::filter_ret_t instr_o = symbol_table_.filter(addr);
    if (!instr_o.has_value()) {
#ifdef PARSER_DEBUG_
      DFLOGIN("%s: filter out event at timestamp %u with address %u\n",
              identifier_.c_str(), t, addr);
#endif
      continue;
    }
    std::string instr = instr_o.value();

    Event event(t, instr);
    std::cout << identifier_ << ": found event --> " << event << std::endl;

    // TODO: gather more info about executed actions?
  }

  return true;
}

bool logparser::NicBmParser::parse_sync_info(std::string &line, bool &sync_pcie,
                                             bool &sync_eth) {
  if (sim_string_utils::consume_and_trim_till_string(line, "sync_pci")) {
    if (!sim_string_utils::consume_and_trim_char(line, '=')) {
#ifdef PARSER_DEBUG_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line.c_str());
#endif
      return false;
    }

    if (sim_string_utils::consume_and_trim_char(line, '1')) {
      sync_pcie = true;
    } else if (sim_string_utils::consume_and_trim_char(line, '0')) {
      sync_pcie = false;
    } else {
#ifdef PARSER_DEBUG_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line.c_str());
#endif
      return false;
    }

    if (!sim_string_utils::consume_and_trim_till_string(line, "sync_eth")) {
#ifdef PARSER_DEBUG_
      DFLOGERR("%s: could not find sync_eth in line '%s'\n", identifier_.c_str(),
               line.c_str());
#endif
      return false;
    }

    if (!sim_string_utils::consume_and_trim_char(line, '=')) {
#ifdef PARSER_DEBUG_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line.c_str());
#endif
      return false;
    }

    if (sim_string_utils::consume_and_trim_char(line, '1')) {
      sync_eth = true;
    } else if (sim_string_utils::consume_and_trim_char(line, '0')) {
      sync_eth = false;
    } else {
#ifdef PARSER_DEBUG_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               identifier_.c_str(), line.c_str());
#endif
      return false;
    }

    return true;
  }

  return false;
}

bool logparser::NicBmParser::parse_mac_address(
    std::string &line, logparser::address_t &mac_address) {
  if (sim_string_utils::consume_and_trim_till_string(line, "mac_addr")) {
    if (!sim_string_utils::consume_and_trim_char(line, '=')) {
#ifdef PARSER_DEBUG_
      DFLOGERR("%s: mac_addr line '%s' has wrong format\n", identifier_.c_str(),
               line.c_str());
#endif
      return false;
    }

    logparser::addressopt_t ad_o = parse_address(line);
    if (!ad_o.has_value()) {
      return false;
    }
    mac_address = ad_o.value();
    return true;
  }
  return false;
}

bool logparser::NicBmParser::parse_off_len_val_comma(std::string &line, address_t &off, size_t &len, address_t &val) {
  logparser::addressopt_t ao;

  // parse off
  if (!sim_string_utils::consume_and_trim_till_string(line, "off=0x")) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }
  ao = parse_address(line);
  if (!ao.has_value()) {
    return false;
  }
  off = ao.value();


  // TODO: check reordering
  // parse len
  if (!sim_string_utils::consume_and_trim_till_string(line, "len=")
      || !sim_string_utils::parse_uint_trim(line, 10, &len)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse len= in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }

  // parse val
  if (!sim_string_utils::consume_and_trim_till_string(line, "val=0x")) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }
  ao = parse_address(line);
  if (!ao.has_value()) {
    return false;
  }
  val = ao.value();

  return true;
}

bool logparser::NicBmParser::parse_op_addr_len_pending(std::string &line, logparser::address_t &op,
                                 logparser::address_t &addr, size_t &len, size_t &pending) {
  logparser::addressopt_t ao;

  // parse op
  if (!sim_string_utils::consume_and_trim_till_string(line, "op 0x")) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse op 0x in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }
  ao = parse_address(line);
  if (!ao.has_value()) {
    return false;
  }
  op = ao.value();

  // parse addr
  if (!sim_string_utils::consume_and_trim_till_string(line, "addr ")) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse addr in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }
  ao = parse_address(line);
  if (!ao.has_value()) {
    return false;
  }
  addr = ao.value();

  // parse len
  if (!sim_string_utils::consume_and_trim_till_string(line, "len ")
      || !sim_string_utils::parse_uint_trim(line, 10, &len)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse len in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }

  // parse pending
  if (!sim_string_utils::consume_and_trim_till_string(line, "pending ")
      || !sim_string_utils::parse_uint_trim(line, 10, &pending)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse pending in line '%s'\n", identifier_.c_str(), line.c_str());
#endif
    return false;
  }
  
  return true;
}

bool logparser::NicBmParser::parse(const std::string &log_file_path) {
  auto line_reader_opt = reader::LineReader::create(log_file_path);
  if (!line_reader_opt.has_value()) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return false;
  }
  auto line_reader = std::move(line_reader_opt.value());

  logparser::address_t mac_address = 0;
  bool sync_pci = false;
  bool sync_eth = false;

  // parse mac address and sync informations
  std::string line;
  if (line_reader.get_next_line(line, true)) {
    if (!parse_mac_address(line, mac_address)) {
      return false;
    }
#ifdef PARSER_DEBUG_
    DFLOGIN("%s: found mac_addr=%lx\n", identifier_.c_str(), mac_address);
#endif
  }
  if (line_reader.get_next_line(line, true)) {
    if (!parse_sync_info(line, sync_pci, sync_eth)) {
      return false;
    }
#ifdef PARSER_DEBUG_
    DFLOGIN("%s: found sync_pcie=%d sync_eth=%d\n", identifier_.c_str(),
            sync_pci, sync_eth);
#endif
  }

  logparser::address_t off, val, op, addr;
  size_t len, pending;
  // parse the actual events of interest
  while (line_reader.get_next_line(line, true)) {
    sim_string_utils::trim(line);
    if (sim_string_utils::consume_and_trim_till_string(
            line, "main_time")) {  // main parsing
      if (!sim_string_utils::consume_and_trim_string(line, " = ")) {
#ifdef PARSER_DEBUG_
        DFLOGERR("%s: main line '%s' has wrong format", identifier_.c_str(),
                 line.c_str());
#endif
        continue;
      }
      logparser::timestampopt_t to = parse_timestamp(line);
      if (!to.has_value()) {
#ifdef PARSER_DEBUG_
        DFLOGERR("%s: could not parse timestamp in line '%s'",
                 identifier_.c_str(), line.c_str());
#endif
        continue;
      }

      if (!sim_string_utils::consume_and_trim_till_string(line, "nicbm")) {
#ifdef PARSER_DEBUG_
        DFLOGERR("%s: line '%s' has wrong format for parsing event info",
                 identifier_.c_str(), line.c_str());
#endif
        continue;
      }

      if (sim_string_utils::consume_and_trim_till_string(line, "read(")) {
        if (!parse_off_len_val_comma(line, off, len, val)) {
          continue;
        }
        // TODO: event
      } else if (sim_string_utils::consume_and_trim_till_string(line, "write(")) {
        if (!parse_off_len_val_comma(line, off, len, val)) {
          continue;
        }
        // TODO: event
      } else if (sim_string_utils::consume_and_trim_till_string(line, "issuing dma")) {
        if (!parse_op_addr_len_pending(line, op, addr, len, pending)) {
          continue;
        }
        // TODO: event
      else if (sim_string_utils::consume_and_trim_till_string(line, "executing dma")) {
        if (!parse_op_addr_len_pending(line, op, addr, len, pending)) {
          continue;
        }
        // TODO event
      } else if (sim_string_utils::consume_and_trim_till_string(line, "completed dma read")) {
        if (!parse_op_addr_len_pending(line, op, addr, len, pending)) {
          continue;
        }
        // TODO event
      } else if (sim_string_utils::consume_and_trim_till_string(line, "completed dma write")) {
        if (!parse_op_addr_len_pending(line, op, addr, len, pending)) {
          continue;
        }
        // TODO: event
      } else if (sim_string_utils::consume_and_trim_till_string(line, "issue MSI-X interrupt vec ")) {
        // TODO event!!
        sim_string_utils::parse_uint_trim();
        if (p.consume_dec(id)) {
          yield(std::make_shared<e_nic_msix>(ts, id));
        }
      } else if (sim_string_utils::consume_and_trim_till_string(line, "eth tx: len ")) {
        if (p.consume_dec(len)) {
          yield(std::make_shared<e_nic_tx>(ts, len));
        }
        // TODO: event
      } else if (sim_string_utils::consume_and_trim_till_string(line, "eth rx: port 0 len ")) {
        if (p.consume_dec(len)) {
          yield(std::make_shared<e_nic_rx>(ts, len));
        }
        // TODO: event
      } else {
#ifdef PARSER_DEBUG_
        DFLOGERR("%s: line '%s' did not match any expected main line\n",
                 identifier_.c_str(), line.c_str());
#endif
        continue;
      }

      // TODO: create event

    } else if (sim_string_utils::consume_and_trim_till_string(
                   line, "exit main_time")) {  // end of event loop
      // TODO: may parse nic statistics as well
      break;
    } else {
#ifdef PARSER_DEBUG_
      DFLOGWARN("%s: could not parse given line '%s'", identifier_.c_str(),
                line.c_str());
#endif
      continue;
    }
  }

  return true;

  uint64_t ts;
  if (!p.consume_dec(ts))
    return;

  if (!p.consume_str(" nicbm: "))
    return;

  uint64_t id, addr, len, val;
  if (p.consume_str("read(off=0x")) {
    if (p.consume_hex(addr) && p.consume_str(", len=") && p.consume_dec(len) &&
        p.consume_str(", val=0x") && p.consume_hex(val)) {
      yield(std::make_shared<e_nic_mmio_r>(ts, addr, len, val));
    }
  } else if (p.consume_str("write(off=0x")) {
    if (p.consume_hex(addr) && p.consume_str(", len=") && p.consume_dec(len) &&
        p.consume_str(", val=0x") && p.consume_hex(val)) {
      yield(std::make_shared<e_nic_mmio_w>(ts, addr, len, val));
    }
  } else if (p.consume_str("issuing dma op 0x")) {
    if (p.consume_hex(id) && p.consume_str(" addr ") && p.consume_hex(addr) &&
        p.consume_str(" len ") && p.consume_hex(len)) {
      yield(std::make_shared<e_nic_dma_i>(ts, id, addr, len));
    }
  } else if (p.consume_str("completed dma read op 0x") ||
             p.consume_str("completed dma write op 0x")) {
    if (p.consume_hex(id) && p.consume_str(" addr ") && p.consume_hex(addr) &&
        p.consume_str(" len ") && p.consume_hex(len)) {
      yield(std::make_shared<e_nic_dma_c>(ts, id));
    }
  } else if (p.consume_str("issue MSI-X interrupt vec ")) {
    if (p.consume_dec(id)) {
      yield(std::make_shared<e_nic_msix>(ts, id));
    }
  } else if (p.consume_str("eth tx: len ")) {
    if (p.consume_dec(len)) {
      yield(std::make_shared<e_nic_tx>(ts, len));
    }
  } else if (p.consume_str("eth rx: port 0 len ")) {
    if (p.consume_dec(len)) {
      yield(std::make_shared<e_nic_rx>(ts, len));
    }
  }
}
