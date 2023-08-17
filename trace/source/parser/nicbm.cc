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

//#define PARSER_DEBUG_NICBM_ 1

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

bool NicBmParser::ParseSyncInfo (bool &sync_pcie, bool &sync_eth)
{
  if (line_reader_.ConsumeAndTrimTillString("sync_pci"))
  {
    if (!line_reader_.ConsumeAndTrimChar('='))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.GetCurString().c_str());
#endif
      return false;
    }

    if (line_reader_.ConsumeAndTrimChar('1'))
    {
      sync_pcie = true;
    } else if (line_reader_.ConsumeAndTrimChar('0'))
    {
      sync_pcie = false;
    } else
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
      return false;
    }

    if (!line_reader_.ConsumeAndTrimTillString("sync_eth"))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: could not find sync_eth in line '%s'\n",
               name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
      return false;
    }

    if (!line_reader_.ConsumeAndTrimChar('='))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
      return false;
    }

    if (line_reader_.ConsumeAndTrimChar('1'))
    {
      sync_eth = true;
    } else if (line_reader_.ConsumeAndTrimChar('0'))
    {
      sync_eth = false;
    } else
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
      return false;
    }

    return true;
  }

  std::cout << "not entered first if" << std::endl;

  return false;
}

bool NicBmParser::ParseMacAddress (uint64_t &address)
{
  if (line_reader_.ConsumeAndTrimTillString("mac_addr"))
  {
    if (!line_reader_.ConsumeAndTrimChar('='))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: mac_addr line '%s' has wrong format\n", name_.c_str(),
               line_reader_.GetRawLine().c_str());
#endif
      return false;
    }

    if (!ParseAddress(address))
    {
      return false;
    }
    return true;
  }
  return false;
}

bool NicBmParser::ParseOffLenValComma (uint64_t &off, size_t &len,
                                       uint64_t &val)
{
  // parse off
  if (!line_reader_.ConsumeAndTrimTillString("off=0x"))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(off))
  {
    return false;
  }

  // parse len
  if (!line_reader_.ConsumeAndTrimTillString("len=") ||
      !line_reader_.ParseUintTrim(10, len))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len= in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }

  // parse val
  if (!line_reader_.ConsumeAndTrimTillString("val=0x"))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(val))
  {
    return false;
  }

  return true;
}

bool NicBmParser::ParseOpAddrLenPending (uint64_t &op, uint64_t &addr,
                                         size_t &len, size_t &pending,
                                         bool with_pending)
{
  // parse op
  if (!line_reader_.ConsumeAndTrimTillString("op 0x"))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse op 0x in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(op))
  {
    return false;
  }

  // parse addr
  if (!line_reader_.ConsumeAndTrimTillString("addr "))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse addr in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }
  if (!ParseAddress(addr))
  {
    return false;
  }

  // parse len
  if (!line_reader_.ConsumeAndTrimTillString("len ") ||
      !line_reader_.ParseUintTrim(10, len))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }

  if (!with_pending)
  {
    return true;
  }

  // parse pending
  if (!line_reader_.ConsumeAndTrimTillString("pending ") ||
      !line_reader_.ParseUintTrim(10, pending))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse pending in line '%s'\n", name_.c_str(),
             line_reader_.GetRawLine().c_str());
#endif
    return false;
  }

  return true;
}

concurrencpp::result<void>
NicBmParser::produce (std::shared_ptr<concurrencpp::executor> resume_executor,
                      std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan)
{
  throw_if_empty (resume_executor, resume_executor_null);
  throw_if_empty (tar_chan, channel_is_null);

  std::cout << "try open nicbm" << std::endl;
  if (not line_reader_.OpenFile(log_file_path_))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not create reader\n", name_.c_str());
#endif
    co_return;
  }

  uint64_t mac_address = 0;
  bool sync_pci = false;
  bool sync_eth = false;

  // parse mac address and sync information
  if (co_await line_reader_.NextLine())
  {
    if (!ParseMacAddress(mac_address))
    {
      co_return;
    }
#ifdef PARSER_DEBUG_NICBM_
    DFLOGIN("%s: found mac_addr=%lx\n", name_.c_str(), mac_address);
#endif
  }
  if (co_await line_reader_.NextLine())
  {
    if (!ParseSyncInfo(sync_pci, sync_eth))
    {
      co_return;
    }
#ifdef PARSER_DEBUG_NICBM_
    DFLOGIN("%s: found sync_pcie=%d sync_eth=%d\n", name_.c_str(),
            sync_pci, sync_eth);
#endif
  }

  std::shared_ptr<Event> event_ptr;
  uint64_t timestamp, off, val, op, addr, vec, pending;
  int port;
  size_t len;
  // parse the actual events of interest
  while (co_await line_reader_.NextLine())
  {
    line_reader_.TrimL();
    if (line_reader_.ConsumeAndTrimTillString(
        "exit main_time"))
    {  // end of event loop
      // TODO: may parse nic statistics as well
#ifdef PARSER_DEBUG_NICBM_
      DFLOGIN("%s: found exit main_time %s\n", name_.c_str(),
              line_reader_.GetRawLine().c_str());
#endif
      continue;
    } else if (line_reader_.ConsumeAndTrimTillString(
        "poll_h2d: peer terminated"))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGIN("%s: found poll_h2d: peer terminated\n", name_.c_str());
#endif
      continue;
    }

    // main parsing
    if (line_reader_.ConsumeAndTrimTillString(
        "main_time"))
    {  // main parsing
      if (!line_reader_.ConsumeAndTrimString(" = "))
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: main line '%s' has wrong format\n", name_.c_str(),
                 line_reader_.GetRawLine().c_str());
#endif
        continue;
      }

      if (!ParseTimestamp(timestamp))
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: could not parse timestamp in line '%s'",
                 name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
        continue;
      }

      if (!line_reader_.ConsumeAndTrimTillString("nicbm"))
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: line '%s' has wrong format for parsing event info",
                 name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
        continue;
      }

      if (line_reader_.ConsumeAndTrimTillString("read("))
      {
        if (!ParseOffLenValComma(off, len, val))
        {
          continue;
        }
        event_ptr = std::make_shared<NicMmioR> (timestamp, GetIdent(),
                                                GetName(), off, len, val);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("write("))
      {
        if (!ParseOffLenValComma(off, len, val))
        {
          continue;
        }
        event_ptr = std::make_shared<NicMmioW> (timestamp, GetIdent(),
                                                GetName(), off, len, val);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("issuing dma"))
      {
        if (!ParseOpAddrLenPending(op, addr, len, pending, true))
        {
          continue;
        }
        event_ptr = std::make_shared<NicDmaI> (timestamp, GetIdent(),
                                               GetName(), op, addr, len);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("executing dma"))
      {
        if (!ParseOpAddrLenPending(op, addr, len, pending, true))
        {
          continue;
        }
        event_ptr = std::make_shared<NicDmaEx> (timestamp, GetIdent(),
                                                GetName(), op, addr, len);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("enqueuing dma"))
      {
        if (!ParseOpAddrLenPending(op, addr, len, pending, true))
        {
          continue;
        }
        event_ptr = std::make_shared<NicDmaEn> (timestamp, GetIdent(),
                                                GetName(), op, addr, len);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("completed dma"))
      {
        if (line_reader_.ConsumeAndTrimTillString("read"))
        {
          if (!ParseOpAddrLenPending(op, addr, len, pending, false))
          {
            continue;
          }
          event_ptr = std::make_shared<NicDmaCR> (timestamp, GetIdent(),
                                                  GetName(), op, addr, len);
          co_await tar_chan->Push (resume_executor, event_ptr);

        } else if (line_reader_.ConsumeAndTrimTillString("write"))
        {
          if (!ParseOpAddrLenPending(op, addr, len, pending, false))
          {
            continue;
          }
          event_ptr = std::make_shared<NicDmaCW> (timestamp, GetIdent(),
                                                  GetName(), op, addr, len);
          co_await tar_chan->Push (resume_executor, event_ptr);
        }
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("issue MSI"))
      {
        bool isX;
        if (line_reader_.ConsumeAndTrimTillString("-X interrupt vec "))
        {
          isX = true;
        } else if (line_reader_.ConsumeAndTrimTillString(
            "interrupt vec "))
        {
          isX = false;
        } else
        {
          continue;
        }
        if (!line_reader_.ParseUintTrim(10, vec))
        {
          continue;
        }
        event_ptr = std::make_shared<NicMsix> (timestamp, GetIdent(),
                                               GetName(), vec, isX);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("eth"))
      {
        if (line_reader_.ConsumeAndTrimTillString("tx: len "))
        {
          if (!line_reader_.ParseUintTrim(10, len))
          {
            continue;
          }
          event_ptr = std::make_shared<NicTx> (timestamp, GetIdent(),
                                               GetName(), len);
          co_await tar_chan->Push (resume_executor, event_ptr);
          continue;

        } else if (line_reader_.ConsumeAndTrimTillString("rx: port "))
        {
          if (!line_reader_.ParseInt(port)
              || !line_reader_.ConsumeAndTrimTillString("len ")
              || !line_reader_.ParseUintTrim(10, len))
          {
            continue;
          }
          event_ptr = std::make_shared<NicRx> (timestamp, GetIdent(),
                                               GetName(), port, len);
          co_await tar_chan->Push (resume_executor, event_ptr);
        }
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString(
          "set intx interrupt"))
      {
        if (!ParseAddress(addr))
        {
          continue;
        }
        event_ptr = std::make_shared<SetIX> (timestamp, GetIdent(),
                                             GetName(), addr);
        co_await tar_chan->Push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.ConsumeAndTrimTillString("dma write data"))
      {
        // ignrore this event, maybe parse data if it turns out to be helpful
        continue;

      } else
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: line '%s' did not match any expected main line\n",
                 name_.c_str(), line_reader_.GetRawLine().c_str());
#endif
        continue;
      }
    } else
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGWARN("%s: could not parse given line '%s'\n", name_.c_str(),
                line_reader_.GetRawLine().c_str());
#endif
      continue;
    }
  }

  co_return;
}
