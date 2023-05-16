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

bool NicBmParser::parse_sync_info (bool &sync_pcie, bool &sync_eth)
{
  if (line_reader_.consume_and_trim_till_string ("sync_pci"))
  {
    if (!line_reader_.consume_and_trim_char ('='))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.get_cur_string().c_str());
#endif
      return false;
    }

    if (line_reader_.consume_and_trim_char ('1'))
    {
      sync_pcie = true;
    } else if (line_reader_.consume_and_trim_char ('0'))
    {
      sync_pcie = false;
    } else
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (!line_reader_.consume_and_trim_till_string ("sync_eth"))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: could not find sync_eth in line '%s'\n",
               name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (!line_reader_.consume_and_trim_char ('='))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (line_reader_.consume_and_trim_char ('1'))
    {
      sync_eth = true;
    } else if (line_reader_.consume_and_trim_char ('0'))
    {
      sync_eth = false;
    } else
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: sync_pcie/sync_eth line '%s' has wrong format\n",
               name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    return true;
  }

  std::cout << "not entered first if" << std::endl;

  return false;
}

bool NicBmParser::parse_mac_address (uint64_t &mac_address)
{
  if (line_reader_.consume_and_trim_till_string ("mac_addr"))
  {
    if (!line_reader_.consume_and_trim_char ('='))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGERR("%s: mac_addr line '%s' has wrong format\n", name_.c_str(),
               line_reader_.get_raw_line().c_str());
#endif
      return false;
    }

    if (!parse_address (mac_address))
    {
      return false;
    }
    return true;
  }
  return false;
}

bool NicBmParser::parse_off_len_val_comma (uint64_t &off, uint64_t &len,
                                           uint64_t &val)
{
  // parse off
  if (!line_reader_.consume_and_trim_till_string ("off=0x"))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address (off))
  {
    return false;
  }

  // parse len
  if (!line_reader_.consume_and_trim_till_string ("len=") ||
      !line_reader_.parse_uint_trim (10, len))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len= in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  // parse val
  if (!line_reader_.consume_and_trim_till_string ("val=0x"))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse off=0x in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address (val))
  {
    return false;
  }

  return true;
}

bool NicBmParser::parse_op_addr_len_pending (uint64_t &op, uint64_t &addr,
                                             uint64_t &len, uint64_t &pending,
                                             bool with_pending)
{
  // parse op
  if (!line_reader_.consume_and_trim_till_string ("op 0x"))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse op 0x in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address (op))
  {
    return false;
  }

  // parse addr
  if (!line_reader_.consume_and_trim_till_string ("addr "))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse addr in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  if (!parse_address (addr))
  {
    return false;
  }

  // parse len
  if (!line_reader_.consume_and_trim_till_string ("len ") ||
      !line_reader_.parse_uint_trim (10, len))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse len in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
#endif
    return false;
  }

  if (!with_pending)
  {
    return true;
  }

  // parse pending
  if (!line_reader_.consume_and_trim_till_string ("pending ") ||
      !line_reader_.parse_uint_trim (10, pending))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not parse pending in line '%s'\n", name_.c_str(),
             line_reader_.get_raw_line().c_str());
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

  if (!line_reader_.open_file (log_file_path_))
  {
#ifdef PARSER_DEBUG_NICBM_
    DFLOGERR("%s: could not create reader\n", name_.c_str());
#endif
    co_return;
  }

  uint64_t mac_address = 0;
  bool sync_pci = false;
  bool sync_eth = false;

  // parse mac address and sync informations
  if (line_reader_.next_line ())
  {
    if (!parse_mac_address (mac_address))
    {
      co_return;
    }
#ifdef PARSER_DEBUG_NICBM_
    DFLOGIN("%s: found mac_addr=%lx\n", name_.c_str(), mac_address);
#endif
  }
  if (line_reader_.next_line ())
  {
    if (!parse_sync_info (sync_pci, sync_eth))
    {
      co_return;
    }
#ifdef PARSER_DEBUG_NICBM_
    DFLOGIN("%s: found sync_pcie=%d sync_eth=%d\n", name_.c_str(),
            sync_pci, sync_eth);
#endif
  }

  std::shared_ptr<Event> event_ptr;
  uint64_t timestamp, off, val, op, addr, vec, len, pending, port;
  // parse the actual events of interest
  while (line_reader_.next_line ())
  {
    line_reader_.trimL ();
    if (line_reader_.consume_and_trim_till_string (
            "exit main_time"))
    {  // end of event loop
      // TODO: may parse nic statistics as well
#ifdef PARSER_DEBUG_NICBM_
      DFLOGIN("%s: found exit main_time %s\n", name_.c_str(),
              line_reader_.get_raw_line().c_str());
#endif
      continue;
    } else if (line_reader_.consume_and_trim_till_string (
            "poll_h2d: peer terminated"))
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGIN("%s: found poll_h2d: peer terminated\n", name_.c_str());
#endif
      continue;
    }

    // main parsing
    if (line_reader_.consume_and_trim_till_string (
            "main_time"))
    {  // main parsing
      if (!line_reader_.consume_and_trim_string (" = "))
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: main line '%s' has wrong format\n", name_.c_str(),
                 line_reader_.get_raw_line().c_str());
#endif
        continue;
      }

      if (!parse_timestamp (timestamp))
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: could not parse timestamp in line '%s'",
                 name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }

      if (!line_reader_.consume_and_trim_till_string ("nicbm"))
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: line '%s' has wrong format for parsing event info",
                 name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }

      if (line_reader_.consume_and_trim_till_string ("read("))
      {
        if (!parse_off_len_val_comma (off, len, val))
        {
          continue;
        }
        event_ptr = std::make_shared<NicMmioR> (timestamp, get_ident(),
                                                get_name(), off, len, val);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("write("))
      {
        if (!parse_off_len_val_comma (off, len, val))
        {
          continue;
        }
        event_ptr = std::make_shared<NicMmioW> (timestamp, get_ident (),
                                                get_name (), off, len, val);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("issuing dma"))
      {
        if (!parse_op_addr_len_pending (op, addr, len, pending, true))
        {
          continue;
        }
        event_ptr = std::make_shared<NicDmaI> (timestamp, get_ident (),
                                               get_name (), op, addr, len);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("executing dma"))
      {
        if (!parse_op_addr_len_pending (op, addr, len, pending, true))
        {
          continue;
        }
        event_ptr = std::make_shared<NicDmaEx> (timestamp, get_ident (),
                                                get_name (), op, addr, len);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("enqueuing dma"))
      {
        if (!parse_op_addr_len_pending (op, addr, len, pending, true))
        {
          continue;
        }
        event_ptr = std::make_shared<NicDmaEn> (timestamp, get_ident (),
                                                get_name (), op, addr, len);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("completed dma"))
      {
        if (line_reader_.consume_and_trim_till_string ("read"))
        {
          if (!parse_op_addr_len_pending (op, addr, len, pending, false))
          {
            continue;
          }
          event_ptr = std::make_shared<NicDmaCR> (timestamp, get_ident (),
                                                  get_name (), op, addr, len);
          co_await tar_chan->push (resume_executor, event_ptr);

        } else if (line_reader_.consume_and_trim_till_string ("write"))
        {
          if (!parse_op_addr_len_pending (op, addr, len, pending, false))
          {
            continue;
          }
          event_ptr = std::make_shared<NicDmaCW> (timestamp, get_ident (),
                                                  get_name (), op, addr, len);
          co_await tar_chan->push (resume_executor, event_ptr);
        }
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("issue MSI"))
      {
        bool isX;
        if (line_reader_.consume_and_trim_till_string ("-X interrupt vec "))
        {
          isX = true;
        } else if (line_reader_.consume_and_trim_till_string (
                "interrupt vec "))
        {
          isX = false;
        } else
        {
          continue;
        }
        if (!line_reader_.parse_uint_trim (10, vec))
        {
          continue;
        }
        event_ptr = std::make_shared<NicMsix> (timestamp, get_ident (),
                                               get_name (), vec, isX);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("eth"))
      {
        if (line_reader_.consume_and_trim_till_string ("tx: len "))
        {
          if (!line_reader_.parse_uint_trim (10, len))
          {
            continue;
          }
          event_ptr = std::make_shared<NicTx> (timestamp, get_ident (),
                                               get_name (), len);
          co_await tar_chan->push (resume_executor, event_ptr);
          continue;

        } else if (line_reader_.consume_and_trim_till_string ("rx: port "))
        {
          if (!line_reader_.parse_uint_trim (10, port)
              || !line_reader_.consume_and_trim_till_string ("len ")
              || !line_reader_.parse_uint_trim (10, len))
          {
            continue;
          }
          event_ptr = std::make_shared<NicRx> (timestamp, get_ident (),
                                               get_name (), port, len);
          co_await tar_chan->push (resume_executor, event_ptr);
        }
        continue;

      } else if (line_reader_.consume_and_trim_till_string (
              "set intx interrupt"))
      {
        if (!parse_address (addr))
        {
          continue;
        }
        event_ptr = std::make_shared<SetIX> (timestamp, get_ident (),
                                             get_name (), addr);
        co_await tar_chan->push (resume_executor, event_ptr);
        continue;

      } else if (line_reader_.consume_and_trim_till_string ("dma write data"))
      {
        // ignrore this event, maybe parse data if it turns out to be helpful
        continue;

      } else
      {
#ifdef PARSER_DEBUG_NICBM_
        DFLOGERR("%s: line '%s' did not match any expected main line\n",
                 name_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }
    } else
    {
#ifdef PARSER_DEBUG_NICBM_
      DFLOGWARN("%s: could not parse given line '%s'\n", name_.c_str(),
                line_reader_.get_raw_line().c_str());
#endif
      continue;
    }
  }

  co_return;
}
