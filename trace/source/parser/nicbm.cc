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

// #define PARSER_DEBUG_NICBM_ 1

#include "parser/parser.h"
#include "util/log.h"
#include "util/exception.h"

/*
 * NOTE: in the following is a list of prints from the nicbm which are currently
 *       not parsed by this parser.
 *
 * - "issue_dma: write too big (%zu), can only fit up to (%zu)\n",op.len_,
 *   maxlen - sizeof(struct SimbricksProtoPcieH2DReadcomp)
 * - "issue_dma: write too big (%zu), can only fit up to (%zu)\n", op.len_,
 *   maxlen
 * - sizeof(*write)
 * - "D2NAlloc: entry successfully allocated\n"
 * - "D2NAlloc: warning waiting for entry (%zu)\n",nicif_.pcie.base.out_pos
 * - "D2HAlloc: entry successfully allocated\n"
 * - "D2HAlloc: warning waiting for entry (%zu)\n", nicif_.pcie.base.out_pos
 * - "Runner::D2HAlloc: peer already terminated\n"
 * - "[%p] main_time = %lu\n", r, r->TimePs()
 * - "poll_h2d: peer terminated\n"
 * - "poll_h2d: unsupported type=%u\n", type
 * - "poll_n2d: unsupported type=%u", t
 * - "warn: SimbricksNicIfSync failed (t=%lu)\n", main_time_
 * - "exit main_time: %lu\n", main_time_
 * - statistics output at the end....
 */

bool NicBmParser::ParseSyncInfo(LineHandler &line_handler, bool &sync_pcie, bool &sync_eth) {
  if (line_handler.ConsumeAndTrimTillString("sync_pci")) {
    if (!line_handler.ConsumeAndTrimChar('=')) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               GetName().c_str(), line_handler.GetCurString().c_str());
#endif
      return false;
    }

    if (line_handler.ConsumeAndTrimChar('1')) {
      sync_pcie = true;
    } else if (line_handler.ConsumeAndTrimChar('0')) {
      sync_pcie = false;
    } else {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      return false;
    }

    if (!line_handler.ConsumeAndTrimTillString("sync_eth")) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: could not find sync_eth in line '%s'\n",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      return false;
    }

    if (!line_handler.ConsumeAndTrimChar('=')) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      return false;
    }

    if (line_handler.ConsumeAndTrimChar('1')) {
      sync_eth = true;
    } else if (line_handler.ConsumeAndTrimChar('0')) {
      sync_eth = false;
    } else {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      return false;
    }

    return true;
  }

  std::cout << "not entered first if" << std::endl;

  return false;
}

bool NicBmParser::ParseMacAddress(LineHandler &line_handler, uint64_t &address) {
  if (line_handler.ConsumeAndTrimTillString("mac_addr")) {
    if (!line_handler.ConsumeAndTrimChar('=')) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: mac_addr line '%s' has wrong format\n", GetName().c_str(),
               line_handler.GetRawLine().c_str());
#endif
      return false;
    }

    if (!ParseAddress(line_handler, address)) {
      return false;
    }
    return true;
  }
  return false;
}

bool NicBmParser::ParseOffLenValComma(LineHandler &line_handler, uint64_t &off, size_t &len,
                                      uint64_t &val) {
  // parse off
  if (!line_handler.ConsumeAndTrimTillString("off=0x")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(line_handler, off)) {
    return false;
  }

  // parse len
  if (!line_handler.ConsumeAndTrimTillString("len=") ||
      !line_handler.ParseUintTrim(10, len)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len= in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }

  // parse val
  if (!line_handler.ConsumeAndTrimTillString("val=0x")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(line_handler, val)) {
    return false;
  }

  return true;
}

bool NicBmParser::ParseOpAddrLenPending(LineHandler &line_handler, uint64_t &op, uint64_t &addr,
                                        size_t &len, size_t &pending,
                                        bool with_pending) {
  // parse op
  if (!line_handler.ConsumeAndTrimTillString("op 0x")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse op 0x in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(line_handler, op)) {
    return false;
  }

  // parse addr
  if (!line_handler.ConsumeAndTrimTillString("addr ")) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse addr in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(line_handler, addr)) {
    return false;
  }

  // parse len
  if (!line_handler.ConsumeAndTrimTillString("len ") ||
      !line_handler.ParseUintTrim(10, len)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }

  if (!with_pending) {
    return true;
  }

  // parse pending
  if (!line_handler.ConsumeAndTrimTillString("pending ") ||
      !line_handler.ParseUintTrim(10, pending)) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse pending in line '%s'\n", GetName().c_str(),
             line_handler.GetRawLine().c_str());
#endif
    return false;
  }

  return true;
}

concurrencpp::lazy_result<std::shared_ptr<Event>>
NicBmParser::ParseEvent(LineHandler &line_handler) {
  if (line_handler.IsEmpty()) {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not create reader\n", GetName().c_str());
#endif
    co_return nullptr;
  }

  std::shared_ptr<Event> event_ptr;
  uint64_t timestamp, off, val, op, addr, vec, pending;
  bool posted;
  int port;
  size_t len;

  line_handler.TrimL();
  // main parsing
  if (line_handler.ConsumeAndTrimTillString(
      "main_time")) {  // main parsing
    if (!line_handler.ConsumeAndTrimString(" = ")) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: main line '%s' has wrong format\n", GetName().c_str(),
               line_handler.GetRawLine().c_str());
#endif
      co_return nullptr;
    }

    if (!ParseTimestamp(line_handler, timestamp)) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: could not parse timestamp in line '%s'",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      co_return nullptr;
    }

    if (!line_handler.ConsumeAndTrimTillString("nicbm")) {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: line '%s' has wrong format for parsing event info",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      co_return nullptr;
    }

    if (line_handler.ConsumeAndTrimTillString("read(")) {
      if (!ParseOffLenValComma(line_handler, off, len, val)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<NicMmioR>(timestamp, GetIdent(),
                                             GetName(), off, len, val);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("write(")) {
      if (!ParseOffLenValComma(line_handler, off, len, val)) {
        co_return nullptr;
      }
      if (not line_handler.ConsumeAndTrimTillString("posted=")
           or not line_handler.ParseBoolFromUint(10, posted)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<NicMmioW>(timestamp, GetIdent(),
                                             GetName(), off, len, val, posted);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("issuing dma")) {
      if (!ParseOpAddrLenPending(line_handler, op, addr, len, pending, true)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<NicDmaI>(timestamp, GetIdent(),
                                            GetName(), op, addr, len);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("executing dma")) {
      if (!ParseOpAddrLenPending(line_handler, op, addr, len, pending, true)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<NicDmaEx>(timestamp, GetIdent(),
                                             GetName(), op, addr, len);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("enqueuing dma")) {
      if (!ParseOpAddrLenPending(line_handler, op, addr, len, pending, true)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<NicDmaEn>(timestamp, GetIdent(),
                                             GetName(), op, addr, len);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("completed dma")) {
      if (line_handler.ConsumeAndTrimTillString("read")) {
        if (!ParseOpAddrLenPending(line_handler, op, addr, len, pending, false)) {
          co_return nullptr;
        }
        event_ptr = std::make_shared<NicDmaCR>(timestamp, GetIdent(),
                                               GetName(), op, addr, len);
        co_return event_ptr;

      } else if (line_handler.ConsumeAndTrimTillString("write")) {
        if (!ParseOpAddrLenPending(line_handler, op, addr, len, pending, false)) {
          co_return nullptr;
        }
        event_ptr = std::make_shared<NicDmaCW>(timestamp, GetIdent(),
                                               GetName(), op, addr, len);
        co_return event_ptr;
      }
      co_return nullptr;

    } else if (line_handler.ConsumeAndTrimTillString("issue MSI")) {
      bool isX;
      if (line_handler.ConsumeAndTrimTillString("-X interrupt vec ")) {
        isX = true;
      } else if (line_handler.ConsumeAndTrimTillString(
          "interrupt vec ")) {
        isX = false;
      } else {
        co_return nullptr;
      }
      if (!line_handler.ParseUintTrim(10, vec)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<NicMsix>(timestamp, GetIdent(),
                                            GetName(), vec, isX);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("eth")) {
      if (line_handler.ConsumeAndTrimTillString("tx: len ")) {
        if (!line_handler.ParseUintTrim(10, len)) {
          co_return nullptr;
        }
        event_ptr = std::make_shared<NicTx>(timestamp, GetIdent(),
                                            GetName(), len);
        co_return event_ptr;

      } else if (line_handler.ConsumeAndTrimTillString("rx: port ")) {
        if (!line_handler.ParseInt(port)
            || !line_handler.ConsumeAndTrimTillString("len ")
            || !line_handler.ParseUintTrim(10, len)) {
          co_return nullptr;
        }
        event_ptr = std::make_shared<NicRx>(timestamp, GetIdent(),
                                            GetName(), port, len);
        co_return event_ptr;
      }
      co_return nullptr;

    } else if (line_handler.ConsumeAndTrimTillString(
        "set intx interrupt")) {
      if (!ParseAddress(line_handler, addr)) {
        co_return nullptr;
      }
      event_ptr = std::make_shared<SetIX>(timestamp, GetIdent(),
                                          GetName(), addr);
      co_return event_ptr;

    } else if (line_handler.ConsumeAndTrimTillString("dma write data")) {
      // ignrore this event, maybe parse data if it turns out to be helpful
      co_return nullptr;

    } else {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: line '%s' did not match any expected main line\n",
               GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      co_return nullptr;
    }
  } else {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGWARN("%s: could not parse given line '%s'\n", GetName().c_str(),
              line_handler.GetRawLine().c_str());
#endif
    co_return nullptr;
  }

  co_return nullptr;
}
