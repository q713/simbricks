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

// #define PARSER_DEBUG_GEM5_ 1

#include <set>
#include <tuple>

#include "util/log.h"
#include "parser/parser.h"
#include "util/string_util.h"
#include "util/exception.h"

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

// #define PARSER_DEBUG_GEM5_ 1

std::shared_ptr<Event> Gem5Parser::ParseGlobalEvent(LineHandler &line_handler, uint64_t timestamp) {
  // 1473190510000: global: simbricks: processInEvent
  if (line_handler.ConsumeAndTrimTillString("simbricks:")) {
    line_handler.TrimL();
    if (line_handler.ConsumeAndTrimString("processInEvent")) {
      return std::make_shared<SimProcInEvent>(timestamp, GetIdent(),
                                              GetName());
    } else if (line_handler.ConsumeAndTrimString("sending sync message")) {
      return std::make_shared<SimSendSync>(timestamp, GetIdent(),
                                           GetName());
    }
  }
  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemSwitchCpus(LineHandler &line_handler, uint64_t timestamp) {
  // 1473191502750: system.switch_cpus: A0 T0 : 0xffffffff81001bc0    :
  // verw_Mw_or_Rv (unimplemented) : No_OpClass :system.switch_cpus:
  // 1472990805875: system.switch_cpus: A0 T0 : 0xffffffff81107470    :   NOP :
  // IntAlu :

  uint64_t addr;
  if (!line_handler.ConsumeAndTrimTillString("0x") ||
      !line_handler.ParseUintTrim(16, addr)) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse address from line '%s'\n", GetName().c_str(),
              line_handler.GetRawLine().c_str());
#endif
    return nullptr;
  }

  line_handler.TrimL();
  if (line_handler.ConsumeAndTrimChar(':')) {
    line_handler.TrimL();
    // NOTE: purposely ignored lines
    if (line_handler.ConsumeAndTrimString("NOP") ||
        line_handler.ConsumeAndTrimString("MFENCE") ||
        line_handler.ConsumeAndTrimString("LFENCE")) {
      return nullptr;
    }
  }

  if (line_handler.ConsumeAndTrimChar('.')) {
    return std::make_shared<HostInstr>(timestamp, GetIdent(), GetName(),
                                       addr);
  } else {
    // in case the given instruction is a call we expect to be able to
    // translate the address to a symbol name
    auto sym_comp = TraceEnvironment::symtable_filter(addr);
    const std::string *sym_s = sym_comp.first;
    const std::string *comp = sym_comp.second;

    if (not comp or not sym_s) {
      return nullptr;
    }

    return std::make_shared<HostCall>(timestamp, GetIdent(), GetName(),
                                      addr,
                                      sym_s, comp);
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemPcPciHost(LineHandler &line_handler, uint64_t timestamp) {
  // TODO: parse  system.pc.pci_host
  // 1369143199499: system.pc.pci_host: 00:00.0: read: offset=0x4, size=0x2

  uint64_t offset;
  size_t size;
  bool is_read = line_handler.ConsumeAndTrimTillString("read: offset=0x");
  if (is_read ||
      line_handler.ConsumeAndTrimTillString("write: offset=0x")) {
    if (line_handler.ParseUintTrim(16, offset) &&
        line_handler.ConsumeAndTrimString(", size=0x") &&
        line_handler.ParseUintTrim(16, size)) {
      return std::make_shared<HostPciRW>(timestamp, GetIdent(), GetName(),
                                         offset, size, is_read);
    }
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemPcPciHostInterface(LineHandler &line_handler, uint64_t timestamp) {
  // TODO: parse system.pc.pci_host.interface
  // 1473338125374: system.pc.pci_host.interface[00:04.0]: clearInt
  // 1473659826000: system.pc.pci_host.interface[00:04.0]: postInt
  // 1473661882374: system.pc.pci_host.interface[00:04.0]: clearInt
  if (!line_handler.SkipTillWhitespace()) {
    return nullptr;
  }
  line_handler.TrimL();

  if (line_handler.ConsumeAndTrimString("clearInt")) {
    return std::make_shared<HostClearInt>(timestamp, GetIdent(),
                                          GetName());
  } else if (line_handler.ConsumeAndTrimString("postInt")) {
    return std::make_shared<HostPostInt>(timestamp, GetIdent(),
                                         GetName());
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemPcSimbricks(LineHandler &line_handler, uint64_t timestamp) {
  if (!line_handler.SkipTillWhitespace()) {
    return nullptr;
  }
  line_handler.TrimL();

  // TODO: parse simbricks
  // 1369143037374: system.pc.simbricks _0: readConfig:  dev 0 func 0 reg 0x3d 1
  // 1693978886124: system.pc.simbricks _0.pio: simbricks-pci: sending immediate
  // response for posted write 1693980306000: system.pc.simbricks_0:
  // simbricks-pci: received MSI-X intr vec 1 1369143037374:
  // system.pc.simbricks_0: readConfig:  dev 0 func 0 reg 0x3d 1 bytes: data =
  // 0x1 1369146219499: system.pc.simbricks_0: writeConfig: dev 0 func 0 reg 0x4
  // 2 bytes: data = 0x6

  // TODO:!!!!!!!!!!!!!!!!!!!!!!!!!
/*
 980510899250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x30 4 bytes: data = 0xfffff800
 980528061250: system.pc.south_bridge.ide: writeConfig: dev 0x4 func 0 reg 0x30 4 bytes: data = 0
 982032152250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x2c 2 bytes: data = 0
 982049790250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x2e 2 bytes: data = 0
 982067678250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x9 1 bytes: data = 0x80
 988036794250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x6 2 bytes: data = 0x280
 988054157250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x6 2 bytes: data = 0x280
 988071611250: system.pc.south_bridge.ide: readConfig:  dev 0x4 func 0 reg 0x6 2 bytes: data = 0x280
 */

  uint64_t dev, func, reg, bytes, data, offset;
  int bar;
  const bool is_read_conf = line_handler.ConsumeAndTrimString("readConfig:");
  if (is_read_conf || line_handler.ConsumeAndTrimString("writeConfig:")) {
    line_handler.TrimL();
    if (line_handler.ConsumeAndTrimString("dev ") &&
        line_handler.ParseUintTrim(10, dev) &&
        line_handler.ConsumeAndTrimString(" func ") &&
        line_handler.ParseUintTrim(10, func) &&
        line_handler.ConsumeAndTrimString(" reg 0x") &&
        line_handler.ParseUintTrim(16, reg) &&
        line_handler.ConsumeAndTrimChar(' ') &&
        line_handler.ParseUintTrim(10, bytes) &&
        line_handler.ConsumeAndTrimString(" bytes: data = ")) {
      if (line_handler.ConsumeAndTrimString("0x") &&
          line_handler.ParseUintTrim(16, data)) {
        return std::make_shared<HostConf>(timestamp, GetIdent(),
                                          GetName(), dev,
                                          func, reg, bytes, data,
                                          is_read_conf);
      } else if (line_handler.ConsumeAndTrimChar('0')) {
        return std::make_shared<HostConf>(timestamp, GetIdent(),
                                          GetName(), dev,
                                          func, reg, bytes, 0, is_read_conf);
      }
    }
  } else if (line_handler.ConsumeAndTrimString("simbricks-pci:")) {
    uint64_t id, addr, vec;
    size_t size;
    line_handler.TrimL();
    if (line_handler.ConsumeAndTrimString("received ")) {
      if (line_handler.ConsumeAndTrimString("write ") &&
          line_handler.ConsumeAndTrimString("completion id ") &&
          line_handler.ParseUintTrim(10, id)) {
        return std::make_shared<HostMmioCW>(timestamp, GetIdent(),
                                            GetName(),
                                            id);
      } else if (line_handler.ConsumeAndTrimString("read ") &&
          line_handler.ConsumeAndTrimString("completion id ") &&
          line_handler.ParseUintTrim(10, id)) {
        return std::make_shared<HostMmioCR>(timestamp, GetIdent(),
                                            GetName(),
                                            id);
      } else if (line_handler.ConsumeAndTrimString("DMA ")) {
        if (line_handler.ConsumeAndTrimString("write id ") &&
            line_handler.ParseUintTrim(10, id) &&
            line_handler.ConsumeAndTrimString(" addr ") &&
            line_handler.ParseUintTrim(16, addr) &&
            line_handler.ConsumeAndTrimString(" size ") &&
            line_handler.ParseUintTrim(10, size)) {
          return std::make_shared<HostDmaW>(timestamp, GetIdent(),
                                            GetName(),
                                            id, addr, size);
        } else if (line_handler.ConsumeAndTrimString("read id ") &&
            line_handler.ParseUintTrim(10, id) &&
            line_handler.ConsumeAndTrimString(" addr ") &&
            line_handler.ParseUintTrim(16, addr) &&
            line_handler.ConsumeAndTrimString(" size ") &&
            line_handler.ParseUintTrim(10, size)) {
          return std::make_shared<HostDmaR>(timestamp, GetIdent(),
                                            GetName(),
                                            id, addr, size);
        }
      } else if (
          line_handler.ConsumeAndTrimTillString("MSI-X intr vec ") &&
              line_handler.ParseUintTrim(10, vec)) {
        return std::make_shared<HostMsiX>(timestamp, GetIdent(),
                                          GetName(),
                                          vec);
      }

    } else if (line_handler.ConsumeAndTrimString("sending ")) {
      int isReadWrite = 0;
      if (line_handler.ConsumeAndTrimString("read addr ")) {
        isReadWrite = 1;
      } else if (line_handler.ConsumeAndTrimString("write addr ")) {
        isReadWrite = -1;
      } else if (line_handler.ConsumeAndTrimString(
          "immediate response for posted write")) {
        return std::make_shared<HostMmioImRespPoW>(timestamp, GetIdent(),
                                                   GetName());
      }

      if (isReadWrite != 0 && line_handler.ParseUintTrim(16, addr) &&
          line_handler.ConsumeAndTrimString(" size ") &&
          line_handler.ParseUintTrim(10, size) &&
          line_handler.ConsumeAndTrimString(" id ") &&
          line_handler.ParseUintTrim(10, id) &&
          line_handler.ConsumeAndTrimString(" bar ") &&
          line_handler.ParseInt(bar) &&
          line_handler.ConsumeAndTrimString(" offs ") &&
          line_handler.ParseUintTrim(16, offset)) {
        if (isReadWrite == 1) {
          return std::make_shared<HostMmioR>(timestamp, GetIdent(),
                                             GetName(),
                                             id, addr, size, bar, offset);
        } else {
          return std::make_shared<HostMmioW>(timestamp, GetIdent(),
                                             GetName(),
                                             id, addr, size, bar, offset);
        }
      }
    } else if (line_handler.ConsumeAndTrimString("completed DMA id ") &&
        line_handler.ParseUintTrim(10, id)) {
      return std::make_shared<HostDmaC>(timestamp, GetIdent(), GetName(),
                                        id);
    }
    // sending immediate response for posted write
  }

  return nullptr;
}

std::shared_ptr<Event> Gem5Parser::ParseSimbricksEvent(LineHandler &line_handler, uint64_t timestamp) {
  if (line_handler.ConsumeAndTrimChar(':')) {
    line_handler.TrimL();
    if (line_handler.ConsumeAndTrimString("processInEvent")) {
      return std::make_shared<SimProcInEvent>(timestamp, GetIdent(),
                                              GetName());
    } else if (line_handler.ConsumeAndTrimString("sending sync message")) {
      return std::make_shared<SimSendSync>(timestamp, GetIdent(),
                                           GetName());
    }
  }

  return nullptr;
}

concurrencpp::lazy_result<std::shared_ptr<Event>>
Gem5Parser::ParseEvent(LineHandler &line_handler) {
  if (line_handler.IsEmpty()) {
    co_return nullptr;
  }

  std::shared_ptr<Event> event_ptr = nullptr;
  std::string component;
  uint64_t timestamp;
  if (!ParseTimestamp(line_handler, timestamp)) {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse timestamp from line '%s'\n", GetName().c_str(),
              line_handler.GetRawLine().c_str());
#endif
    co_return nullptr;
  }
  if (!line_handler.ConsumeAndTrimChar(':')) {
    co_return nullptr;
  }
  line_handler.TrimL();

  // call parsing function based on component
  if (line_handler.ConsumeAndTrimString("global:") &&
      component_table_.filter("global")) {
    event_ptr = ParseGlobalEvent(line_handler, timestamp);
    if (!event_ptr) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse global event from line '%s'\n",
                GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      co_return nullptr;
    }
    // got event
  } else if (line_handler.ConsumeAndTrimString("system.switch_cpus:") &&
      component_table_.filter("system.switch_cpus")) {
    event_ptr = ParseSystemSwitchCpus(line_handler, timestamp);
    if (!event_ptr) {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN(
          "%s: could not parse system.switch_cpus event from line '%s'\n",
          GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
      co_return nullptr;
    }
    // got event
  } else if (line_handler.ConsumeAndTrimString("system.pc")) {
    if (line_handler.ConsumeAndTrimString(".pci_host")) {
      if (line_handler.ConsumeAndTrimString(".interface") &&
          component_table_.filter("system.pc.pci_host.interface")) {
        event_ptr = ParseSystemPcPciHostInterface(line_handler, timestamp);
        if (!event_ptr) {
#ifdef PARSER_DEBUG_GEM5_
          DFLOGWARN(
              "%s: could not parse system.pc.pci_host.interface event from "
              "line '%s'\n",
              GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
          co_return nullptr;
        }
        // got event
      } else if (component_table_.filter("system.pc.pci_host")) {
        event_ptr = ParseSystemPcPciHost(line_handler, timestamp);
        if (!event_ptr) {
#ifdef PARSER_DEBUG_GEM5_
          DFLOGWARN(
              "%s: could not parse system.pc.pci_host event from "
              "line '%s'\n",
              GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
          co_return nullptr;
        }
      }
    } else if (line_handler.ConsumeAndTrimString(".simbricks") &&
        component_table_.filter("system.pc.simbricks")) {
      event_ptr = ParseSystemPcSimbricks(line_handler, timestamp);
      if (!event_ptr) {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGWARN(
            "%s: could not parse system.pc.simbricks event from line "
            "'%s'\n",
            GetName().c_str(), line_handler.GetRawLine().c_str());
#endif
        co_return nullptr;
      }
      // got event
    }
  }

#ifdef PARSER_DEBUG_GEM5_
  if (not event_ptr) {
    DFLOGWARN("%s: could not parse event in line '%s'\n", GetName().c_str(),
              line_handler.GetRawLine().c_str());
  }
#endif
  co_return event_ptr;
}

#if 0
std::shared_ptr<Event> Gem5Parser::ParseGlobalEvent (uint64_t timestamp)
{
  // 1473190510000: global: simbricks: processInEvent
  if (line_reader_.ConsumeAndTrimTillString("simbricks:"))
  {
    line_reader_.TrimL();
    if (line_reader_.ConsumeAndTrimString("processInEvent"))
    {
      return std::make_shared<SimProcInEvent> (timestamp, GetIdent(),
                                               GetName());
    } else if (line_reader_.ConsumeAndTrimString("sending sync message"))
    {
      return std::make_shared<SimSendSync> (timestamp, GetIdent(),
                                            GetName());
    }
  }
  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemSwitchCpus (uint64_t timestamp)
{
  // 1473191502750: system.switch_cpus: A0 T0 : 0xffffffff81001bc0    :
  // verw_Mw_or_Rv (unimplemented) : No_OpClass :system.switch_cpus:
  // 1472990805875: system.switch_cpus: A0 T0 : 0xffffffff81107470    :   NOP :
  // IntAlu :

  uint64_t addr;
  if (!line_reader_.ConsumeAndTrimTillString("0x") ||
      !line_reader_.ParseUintTrim(16, addr))
  {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse address from line '%s'\n", name_.c_str(),
              line_reader_.GetRawLine().c_str());
#endif
    return nullptr;
  }

  line_reader_.TrimL();
  if (line_reader_.ConsumeAndTrimChar(':'))
  {
    line_reader_.TrimL();
    // NOTE: purposely ignored lines
    if (line_reader_.ConsumeAndTrimString("NOP") ||
        line_reader_.ConsumeAndTrimString("MFENCE") ||
        line_reader_.ConsumeAndTrimString("LFENCE"))
    {
      return nullptr;
    }
  }

  if (line_reader_.ConsumeAndTrimChar('.'))
  {
    return std::make_shared<HostInstr> (timestamp, GetIdent(), GetName(),
                                        addr);
  } else
  {
    // in case the given instruction is a call we expect to be able to
    // translate the address to a symbol name
    auto sym_comp = TraceEnvironment::symtable_filter (addr);
    const std::string *sym_s = sym_comp.first;
    const std::string *comp = sym_comp.second;

    if (not comp or not sym_s)
    {
      return nullptr;
    }

    return std::make_shared<HostCall> (timestamp, GetIdent(), GetName(),
                                       addr,
                                       sym_s, comp);
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemPcPciHost (uint64_t timestamp)
{
  // TODO: parse  system.pc.pci_host
  // 1369143199499: system.pc.pci_host: 00:00.0: read: offset=0x4, size=0x2

  uint64_t offset;
  size_t size;
  bool is_read = line_reader_.ConsumeAndTrimTillString("read: offset=0x");
  if (is_read ||
      line_reader_.ConsumeAndTrimTillString("write: offset=0x"))
  {
    if (line_reader_.ParseUintTrim(16, offset) &&
        line_reader_.ConsumeAndTrimString(", size=0x") &&
        line_reader_.ParseUintTrim(16, size))
    {
      return std::make_shared<HostPciRW> (timestamp, GetIdent(), GetName(),
                                          offset, size, is_read);
    }
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemPcPciHostInterface (uint64_t timestamp)
{
  // TODO: parse system.pc.pci_host.interface
  // 1473338125374: system.pc.pci_host.interface[00:04.0]: clearInt
  // 1473659826000: system.pc.pci_host.interface[00:04.0]: postInt
  // 1473661882374: system.pc.pci_host.interface[00:04.0]: clearInt
  if (!line_reader_.SkipTillWhitespace())
  {
    return nullptr;
  }
  line_reader_.TrimL();

  if (line_reader_.ConsumeAndTrimString("clearInt"))
  {
    return std::make_shared<HostClearInt> (timestamp, GetIdent(),
                                           GetName());
  } else if (line_reader_.ConsumeAndTrimString("postInt"))
  {
    return std::make_shared<HostPostInt> (timestamp, GetIdent(),
                                          GetName());
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::ParseSystemPcSimbricks (uint64_t timestamp)
{
  if (!line_reader_.SkipTillWhitespace())
  {
    return nullptr;
  }
  line_reader_.TrimL();

  // TODO: parse simbricks
  // 1369143037374: system.pc.simbricks _0: readConfig:  dev 0 func 0 reg 0x3d 1
  // 1693978886124: system.pc.simbricks _0.pio: simbricks-pci: sending immediate
  // response for posted write 1693980306000: system.pc.simbricks_0:
  // simbricks-pci: received MSI-X intr vec 1 1369143037374:
  // system.pc.simbricks_0: readConfig:  dev 0 func 0 reg 0x3d 1 bytes: data =
  // 0x1 1369146219499: system.pc.simbricks_0: writeConfig: dev 0 func 0 reg 0x4
  // 2 bytes: data = 0x6

  uint64_t dev, func, reg, bytes, data, offset;
  int bar;
  const bool is_read_conf = line_reader_.ConsumeAndTrimString("readConfig:");
  if (is_read_conf || line_reader_.ConsumeAndTrimString("writeConfig:"))
  {
    line_reader_.TrimL();
    if (line_reader_.ConsumeAndTrimString("dev ") &&
        line_reader_.ParseUintTrim(10, dev) &&
        line_reader_.ConsumeAndTrimString(" func ") &&
        line_reader_.ParseUintTrim(10, func) &&
        line_reader_.ConsumeAndTrimString(" reg 0x") &&
        line_reader_.ParseUintTrim(16, reg) &&
        line_reader_.ConsumeAndTrimChar(' ') &&
        line_reader_.ParseUintTrim(10, bytes) &&
        line_reader_.ConsumeAndTrimString(" bytes: data = "))
    {
      if (line_reader_.ConsumeAndTrimString("0x") &&
          line_reader_.ParseUintTrim(16, data))
      {
        return std::make_shared<HostConf> (timestamp, GetIdent(),
                                           GetName(), dev,
                                           func, reg, bytes, data,
                                           is_read_conf);
      } else if (line_reader_.ConsumeAndTrimChar('0'))
      {
        return std::make_shared<HostConf> (timestamp, GetIdent(),
                                           GetName(), dev,
                                           func, reg, bytes, 0, is_read_conf);
      }
    }
  } else if (line_reader_.ConsumeAndTrimString("simbricks-pci:"))
  {
    uint64_t id, addr, vec;
    size_t size;
    line_reader_.TrimL();
    if (line_reader_.ConsumeAndTrimString("received "))
    {
      if (line_reader_.ConsumeAndTrimString("write ") &&
          line_reader_.ConsumeAndTrimString("completion id ") &&
          line_reader_.ParseUintTrim(10, id))
      {
        return std::make_shared<HostMmioCW> (timestamp, GetIdent(),
                                             GetName(),
                                             id);
      } else if (line_reader_.ConsumeAndTrimString("read ") &&
          line_reader_.ConsumeAndTrimString("completion id ") &&
          line_reader_.ParseUintTrim(10, id))
      {
        return std::make_shared<HostMmioCR> (timestamp, GetIdent(),
                                             GetName(),
                                             id);
      } else if (line_reader_.ConsumeAndTrimString("DMA "))
      {
        if (line_reader_.ConsumeAndTrimString("write id ") &&
            line_reader_.ParseUintTrim(10, id) &&
            line_reader_.ConsumeAndTrimString(" addr ") &&
            line_reader_.ParseUintTrim(16, addr) &&
            line_reader_.ConsumeAndTrimString(" size ") &&
            line_reader_.ParseUintTrim(10, size))
        {
          return std::make_shared<HostDmaW> (timestamp, GetIdent(),
                                             GetName(),
                                             id, addr, size);
        } else if (line_reader_.ConsumeAndTrimString("read id ") &&
            line_reader_.ParseUintTrim(10, id) &&
            line_reader_.ConsumeAndTrimString(" addr ") &&
            line_reader_.ParseUintTrim(16, addr) &&
            line_reader_.ConsumeAndTrimString(" size ") &&
            line_reader_.ParseUintTrim(10, size))
        {
          return std::make_shared<HostDmaR> (timestamp, GetIdent(),
                                             GetName(),
                                             id, addr, size);
        }
      } else if (
          line_reader_.ConsumeAndTrimTillString("MSI-X intr vec ") &&
                  line_reader_.ParseUintTrim(10, vec))
      {
        return std::make_shared<HostMsiX> (timestamp, GetIdent(),
                                           GetName(),
                                           vec);
      }

    } else if (line_reader_.ConsumeAndTrimString("sending "))
    {
      int isReadWrite = 0;
      if (line_reader_.ConsumeAndTrimString("read addr "))
      {
        isReadWrite = 1;
      } else if (line_reader_.ConsumeAndTrimString("write addr "))
      {
        isReadWrite = -1;
      } else if (line_reader_.ConsumeAndTrimString(
          "immediate response for posted write"))
      {
        return std::make_shared<HostMmioImRespPoW> (timestamp, GetIdent(),
                                                    GetName());
      }

      if (isReadWrite != 0 && line_reader_.ParseUintTrim(16, addr) &&
          line_reader_.ConsumeAndTrimString(" size ") &&
          line_reader_.ParseUintTrim(10, size) &&
          line_reader_.ConsumeAndTrimString(" id ") &&
          line_reader_.ParseUintTrim(10, id) &&
          line_reader_.ConsumeAndTrimString(" bar ") &&
          line_reader_.ParseInt(bar) &&
          line_reader_.ConsumeAndTrimString(" offs ") &&
          line_reader_.ParseUintTrim(16, offset))
      {
        if (isReadWrite == 1)
        {
          return std::make_shared<HostMmioR> (timestamp, GetIdent(),
                                              GetName(),
                                              id, addr, size, bar, offset);
        } else
        {
          return std::make_shared<HostMmioW> (timestamp, GetIdent(),
                                              GetName(),
                                              id, addr, size, bar, offset);
        }
      }
    } else if (line_reader_.ConsumeAndTrimString("completed DMA id ") &&
        line_reader_.ParseUintTrim(10, id))
    {
      return std::make_shared<HostDmaC> (timestamp, GetIdent(), GetName(),
                                         id);
    }
    // sending immediate response for posted write
  }

  return nullptr;
}

std::shared_ptr<Event> Gem5Parser::ParseSimbricksEvent (uint64_t timestamp)
{
  if (line_reader_.ConsumeAndTrimChar(':'))
  {
    line_reader_.TrimL();
    if (line_reader_.ConsumeAndTrimString("processInEvent"))
    {
      return std::make_shared<SimProcInEvent> (timestamp, GetIdent(),
                                               GetName());
    } else if (line_reader_.ConsumeAndTrimString("sending sync message"))
    {
      return std::make_shared<SimSendSync> (timestamp, GetIdent(),
                                            GetName());
    }
  }

  return nullptr;
}

concurrencpp::result<void>
Gem5Parser::produce (std::shared_ptr<concurrencpp::executor> resume_executor,
                     std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan)
{
  throw_if_empty (resume_executor, resume_executor_null);
  throw_if_empty (tar_chan, channel_is_null);

  std::cout << "try open gem5" << std::endl;
  if (not line_reader_.OpenFile(log_file_path_))
  {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: could not create reader\n", name_.c_str());
#endif
    co_return;
  }

  std::shared_ptr<Event> event_ptr;
  std::string component;
  uint64_t timestamp;
  while (co_await line_reader_.NextLine())
  {
    if (!ParseTimestamp(timestamp))
    {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse timestamp from line '%s'\n", name_.c_str(),
                line_reader_.GetRawLine().c_str());
#endif
      continue;
    }
    if (!line_reader_.ConsumeAndTrimChar(':'))
    {
      continue;
    }
    line_reader_.TrimL();

    // call parsing function based on component
    if (line_reader_.ConsumeAndTrimString("global:") &&
        component_table_.filter ("global"))
    {
      event_ptr = ParseGlobalEvent(timestamp);
      if (!event_ptr)
      {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGWARN("%s: could not parse global event from line '%s'\n",
                  name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
        continue;
      }
      co_await tar_chan->Push (resume_executor, event_ptr);
      continue;
    } else if (line_reader_.ConsumeAndTrimString("system.switch_cpus:") &&
               component_table_.filter ("system.switch_cpus"))
    {
      event_ptr = ParseSystemSwitchCpus(timestamp);
      if (!event_ptr)
      {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGWARN(
            "%s: could not parse system.switch_cpus event from line '%s'\n",
            name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
        continue;
      }
      co_await tar_chan->Push (resume_executor, event_ptr);
      continue;

    } else if (line_reader_.ConsumeAndTrimString("system.pc"))
    {
      if (line_reader_.ConsumeAndTrimString(".pci_host"))
      {
        if (line_reader_.ConsumeAndTrimString(".interface") &&
            component_table_.filter ("system.pc.pci_host.interface"))
        {
          event_ptr = ParseSystemPcPciHostInterface(timestamp);
          if (!event_ptr)
          {
#ifdef PARSER_DEBUG_GEM5_
            DFLOGWARN(
                "%s: could not parse system.pc.pci_host.interface event from "
                "line '%s'\n",
                name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
            continue;
          }
          co_await tar_chan->Push (resume_executor, event_ptr);
          continue;
        } else if (component_table_.filter ("system.pc.pci_host"))
        {
          event_ptr = ParseSystemPcPciHost(timestamp);
          if (!event_ptr)
          {
#ifdef PARSER_DEBUG_GEM5_
            DFLOGWARN(
                "%s: could not parse system.pc.pci_host event from "
                "line '%s'\n",
                name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
            continue;
          }
          co_await tar_chan->Push (resume_executor, event_ptr);
          continue;
        }
      } else if (line_reader_.ConsumeAndTrimString(".simbricks") &&
                 component_table_.filter ("system.pc.simbricks"))
      {
        event_ptr = ParseSystemPcSimbricks(timestamp);
        if (!event_ptr)
        {
#ifdef PARSER_DEBUG_GEM5_
          DFLOGWARN(
              "%s: could not parse system.pc.simbricks event from line "
              "'%s'\n",
              name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
          continue;
        }
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;
      }
    }

#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse event in line '%s'\n", name_.c_str(),
              line_reader_.GetRawLine().c_str());
#endif
  }

  std::cout << "gem5 parser exited" << std::endl;

  co_return;
}
#endif
