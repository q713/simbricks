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

std::shared_ptr<Event> Gem5Parser::parse_global_event (uint64_t timestamp)
{
  // 1473190510000: global: simbricks: processInEvent
  if (line_reader_.consume_and_trim_till_string ("simbricks:"))
  {
    line_reader_.trimL ();
    if (line_reader_.consume_and_trim_string ("processInEvent"))
    {
      return std::make_shared<SimProcInEvent> (timestamp, get_ident (),
                                               get_name ());
    } else if (line_reader_.consume_and_trim_string ("sending sync message"))
    {
      return std::make_shared<SimSendSync> (timestamp, get_ident (),
                                            get_name ());
    }
  }
  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::parse_system_switch_cpus (uint64_t timestamp)
{
  // 1473191502750: system.switch_cpus: A0 T0 : 0xffffffff81001bc0    :
  // verw_Mw_or_Rv (unimplemented) : No_OpClass :system.switch_cpus:
  // 1472990805875: system.switch_cpus: A0 T0 : 0xffffffff81107470    :   NOP :
  // IntAlu :

  uint64_t addr;
  if (!line_reader_.consume_and_trim_till_string ("0x") ||
      !line_reader_.parse_uint_trim (16, addr))
  {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse address from line '%s'\n", name_.c_str(),
              line_reader_.get_raw_line().c_str());
#endif
    return nullptr;
  }

  line_reader_.trimL ();
  if (line_reader_.consume_and_trim_char (':'))
  {
    line_reader_.trimL ();
    // NOTE: purposely ignored lines
    if (line_reader_.consume_and_trim_string ("NOP") ||
        line_reader_.consume_and_trim_string ("MFENCE") ||
        line_reader_.consume_and_trim_string ("LFENCE"))
    {
      return nullptr;
    }
  }

  if (line_reader_.consume_and_trim_char ('.'))
  {
    // TODO: gather micro operation information? if yes, which informations?
    return std::make_shared<HostInstr> (timestamp, get_ident (), get_name (),
                                        addr);
  } else
  {
    // in case the given instruction is a call we expect to be able to
    // translate the address to a symbol name
    auto sym_comp = trace_environment::symtable_filter (addr);
    const std::string *sym_s = sym_comp.first;
    const std::string *comp = sym_comp.second;

    if (not comp or not sym_s)
    {
      return nullptr;
    }

    return std::make_shared<HostCall> (timestamp, get_ident (), get_name (),
                                       addr,
                                       sym_s, comp);
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::parse_system_pc_pci_host (uint64_t timestamp)
{
  // TODO: parse  system.pc.pci_host
  // 1369143199499: system.pc.pci_host: 00:00.0: read: offset=0x4, size=0x2

  uint64_t offset, size;
  bool is_read = line_reader_.consume_and_trim_till_string ("read: offset=0x");
  if (is_read ||
      line_reader_.consume_and_trim_till_string ("write: offset=0x"))
  {
    if (line_reader_.parse_uint_trim (16, offset) &&
        line_reader_.consume_and_trim_string (", size=0x") &&
        line_reader_.parse_uint_trim (16, size))
    {
      return std::make_shared<HostPciRW> (timestamp, get_ident (), get_name (),
                                          offset, size, is_read);
    }
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::parse_system_pc_pci_host_interface (uint64_t timestamp)
{
  // TODO: parse system.pc.pci_host.interface
  // 1473338125374: system.pc.pci_host.interface[00:04.0]: clearInt
  // 1473659826000: system.pc.pci_host.interface[00:04.0]: postInt
  // 1473661882374: system.pc.pci_host.interface[00:04.0]: clearInt
  if (!line_reader_.skip_till_whitespace ())
  {
    return nullptr;
  }
  line_reader_.trimL ();

  if (line_reader_.consume_and_trim_string ("clearInt"))
  {
    return std::make_shared<HostClearInt> (timestamp, get_ident (),
                                           get_name ());
  } else if (line_reader_.consume_and_trim_string ("postInt"))
  {
    return std::make_shared<HostPostInt> (timestamp, get_ident (),
                                          get_name ());
  }

  return nullptr;
}

std::shared_ptr<Event>
Gem5Parser::parse_system_pc_simbricks (uint64_t timestamp)
{
  if (!line_reader_.skip_till_whitespace ())
  {
    return nullptr;
  }
  line_reader_.trimL ();

  // TODO: parse simbricks
  // 1369143037374: system.pc.simbricks _0: readConfig:  dev 0 func 0 reg 0x3d 1
  // 1693978886124: system.pc.simbricks _0.pio: simbricks-pci: sending immediate
  // response for posted write 1693980306000: system.pc.simbricks_0:
  // simbricks-pci: received MSI-X intr vec 1 1369143037374:
  // system.pc.simbricks_0: readConfig:  dev 0 func 0 reg 0x3d 1 bytes: data =
  // 0x1 1369146219499: system.pc.simbricks_0: writeConfig: dev 0 func 0 reg 0x4
  // 2 bytes: data = 0x6

  uint64_t dev, func, reg, bytes, data, bar, offset;
  bool is_readConf = line_reader_.consume_and_trim_string ("readConfig:");
  if (is_readConf || line_reader_.consume_and_trim_string ("writeConfig:"))
  {
    line_reader_.trimL ();
    if (line_reader_.consume_and_trim_string ("dev ") &&
        line_reader_.parse_uint_trim (10, dev) &&
        line_reader_.consume_and_trim_string (" func ") &&
        line_reader_.parse_uint_trim (10, func) &&
        line_reader_.consume_and_trim_string (" reg 0x") &&
        line_reader_.parse_uint_trim (16, reg) &&
        line_reader_.consume_and_trim_char (' ') &&
        line_reader_.parse_uint_trim (10, bytes) &&
        line_reader_.consume_and_trim_string (" bytes: data = "))
    {
      if (line_reader_.consume_and_trim_string ("0x") &&
          line_reader_.parse_uint_trim (16, data))
      {
        return std::make_shared<HostConf> (timestamp, get_ident (),
                                           get_name (), dev,
                                           func, reg, bytes, data,
                                           is_readConf);
      } else if (line_reader_.consume_and_trim_char ('0'))
      {
        return std::make_shared<HostConf> (timestamp, get_ident (),
                                           get_name (), dev,
                                           func, reg, bytes, 0, is_readConf);
      }
    }
  } else if (line_reader_.consume_and_trim_string ("simbricks-pci:"))
  {
    uint64_t id, addr, size, vec;
    line_reader_.trimL ();
    if (line_reader_.consume_and_trim_string ("received "))
    {
      if (line_reader_.consume_and_trim_string ("write ") &&
          line_reader_.consume_and_trim_string ("completion id ") &&
          line_reader_.parse_uint_trim (10, id))
      {
        return std::make_shared<HostMmioCW> (timestamp, get_ident (),
                                             get_name (),
                                             id);
      } else if (line_reader_.consume_and_trim_string ("read ") &&
                 line_reader_.consume_and_trim_string ("completion id ") &&
                 line_reader_.parse_uint_trim (10, id))
      {
        return std::make_shared<HostMmioCR> (timestamp, get_ident (),
                                             get_name (),
                                             id);
      } else if (line_reader_.consume_and_trim_string ("DMA "))
      {
        if (line_reader_.consume_and_trim_string ("write id ") &&
            line_reader_.parse_uint_trim (10, id) &&
            line_reader_.consume_and_trim_string (" addr ") &&
            line_reader_.parse_uint_trim (16, addr) &&
            line_reader_.consume_and_trim_string (" size ") &&
            line_reader_.parse_uint_trim (10, size))
        {
          return std::make_shared<HostDmaW> (timestamp, get_ident (),
                                             get_name (),
                                             id, addr, size);
        } else if (line_reader_.consume_and_trim_string ("read id ") &&
                   line_reader_.parse_uint_trim (10, id) &&
                   line_reader_.consume_and_trim_string (" addr ") &&
                   line_reader_.parse_uint_trim (16, addr) &&
                   line_reader_.consume_and_trim_string (" size ") &&
                   line_reader_.parse_uint_trim (10, size))
        {
          return std::make_shared<HostDmaR> (timestamp, get_ident (),
                                             get_name (),
                                             id, addr, size);
        }
      } else if (
              line_reader_.consume_and_trim_till_string ("MSI-X intr vec ") &&
              line_reader_.parse_uint_trim (10, vec))
      {
        return std::make_shared<HostMsiX> (timestamp, get_ident (),
                                           get_name (),
                                           vec);
      }

    } else if (line_reader_.consume_and_trim_string ("sending "))
    {
      int isReadWrite = 0;
      if (line_reader_.consume_and_trim_string ("read addr "))
      {
        isReadWrite = 1;
      } else if (line_reader_.consume_and_trim_string ("write addr "))
      {
        isReadWrite = -1;
      } else if (line_reader_.consume_and_trim_string (
              "immediate response for posted write"))
      {
        return std::make_shared<HostMmioImRespPoW> (timestamp, get_ident (),
                                                    get_name ());
      }

      if (isReadWrite != 0 && line_reader_.parse_uint_trim (16, addr) &&
          line_reader_.consume_and_trim_string (" size ") &&
          line_reader_.parse_uint_trim (10, size) &&
          line_reader_.consume_and_trim_string (" id ") &&
          line_reader_.parse_uint_trim (10, id) &&
          line_reader_.consume_and_trim_string (" bar ") &&
          line_reader_.parse_uint_trim (10, bar) &&
          line_reader_.consume_and_trim_string (" offs ") &&
          line_reader_.parse_uint_trim (16, offset))
      {
        if (isReadWrite == 1)
        {
          return std::make_shared<HostMmioR> (timestamp, get_ident (),
                                              get_name (),
                                              id, addr, size, bar, offset);
        } else
        {
          return std::make_shared<HostMmioW> (timestamp, get_ident (),
                                              get_name (),
                                              id, addr, size, bar, offset);
        }
      }
    } else if (line_reader_.consume_and_trim_string ("completed DMA id ") &&
               line_reader_.parse_uint_trim (10, id))
    {
      return std::make_shared<HostDmaC> (timestamp, get_ident (), get_name (),
                                         id);
    }
    // sending immediate response for posted write
  }

  return nullptr;
}

std::shared_ptr<Event> Gem5Parser::parse_simbricks_event (uint64_t timestamp)
{
  if (line_reader_.consume_and_trim_char (':'))
  {
    line_reader_.trimL ();
    if (line_reader_.consume_and_trim_string ("processInEvent"))
    {
      return std::make_shared<SimProcInEvent> (timestamp, get_ident (),
                                               get_name ());
    } else if (line_reader_.consume_and_trim_string ("sending sync message"))
    {
      return std::make_shared<SimSendSync> (timestamp, get_ident (),
                                            get_name ());
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

  if (!line_reader_.open_file (log_file_path_))
  {
#ifdef PARSER_DEBUG_GEM5_
    DFLOGERR("%s: could not create reader\n", name_.c_str());
#endif
    co_return;
  }

  std::shared_ptr<Event> event_ptr;
  std::string component;
  uint64_t timestamp;
  while (line_reader_.next_line ())
  {
    if (!parse_timestamp (timestamp))
    {
#ifdef PARSER_DEBUG_GEM5_
      DFLOGWARN("%s: could not parse timestamp from line '%s'\n", name_.c_str(),
                line_reader_.get_raw_line().c_str());
#endif
      continue;
    }
    if (!line_reader_.consume_and_trim_char (':'))
    {
      continue;
    }
    line_reader_.trimL ();

    // call parsing function based on component
    if (line_reader_.consume_and_trim_string ("global:") &&
        component_table_.filter ("global"))
    {
      event_ptr = parse_global_event (timestamp);
      if (!event_ptr)
      {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGWARN("%s: could not parse global event from line '%s'\n",
                  name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }
      co_await tar_chan->push (resume_executor, event_ptr);
      continue;
    } else if (line_reader_.consume_and_trim_string ("system.switch_cpus:") &&
               component_table_.filter ("system.switch_cpus"))
    {
      event_ptr = parse_system_switch_cpus (timestamp);
      if (!event_ptr)
      {
#ifdef PARSER_DEBUG_GEM5_
        DFLOGWARN(
            "%s: could not parse system.switch_cpus event from line '%s'\n",
            name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }
      co_await tar_chan->push (resume_executor, event_ptr);
      continue;

    } else if (line_reader_.consume_and_trim_string ("system.pc"))
    {
      if (line_reader_.consume_and_trim_string (".pci_host"))
      {
        if (line_reader_.consume_and_trim_string (".interface") &&
            component_table_.filter ("system.pc.pci_host.interface"))
        {
          event_ptr = parse_system_pc_pci_host_interface (timestamp);
          if (!event_ptr)
          {
#ifdef PARSER_DEBUG_GEM5_
            DFLOGWARN(
                "%s: could not parse system.pc.pci_host.interface event from "
                "line '%s'\n",
                name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
            continue;
          }
          co_await tar_chan->push (resume_executor, event_ptr);
          continue;
        } else if (component_table_.filter ("system.pc.pci_host"))
        {
          event_ptr = parse_system_pc_pci_host (timestamp);
          if (!event_ptr)
          {
#ifdef PARSER_DEBUG_GEM5_
            DFLOGWARN(
                "%s: could not parse system.pc.pci_host event from "
                "line '%s'\n",
                name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
            continue;
          }
          co_await tar_chan->push (resume_executor, event_ptr);
          continue;
        }
      } else if (line_reader_.consume_and_trim_string (".simbricks") &&
                 component_table_.filter ("system.pc.simbricks"))
      {
        event_ptr = parse_system_pc_simbricks (timestamp);
        if (!event_ptr)
        {
#ifdef PARSER_DEBUG_GEM5_
          DFLOGWARN(
              "%s: could not parse system.pc.simbricks event from line "
              "'%s'\n",
              name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
          continue;
        }
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;
      }
    }

#ifdef PARSER_DEBUG_GEM5_
    DFLOGWARN("%s: could not parse event in line '%s'\n", name_.c_str(),
              line_reader_.get_raw_line().c_str());
#endif
  }

  co_return;
}