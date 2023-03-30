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

#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "lib/utils/string_util.h"
#include "trace/corobelt/coroutine.h"
#include "trace/reader/reader.h"

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_

using event_t = std::shared_ptr<Event>;
using task_t = sim::coroutine::task<void>;
using chan_t = sim::coroutine::unbuffered_single_chan<event_t>;

struct event_stream_parser : public sim::coroutine::producer<event_t> {
  bool parse_ident_name_ts(size_t &parser_ident, std::string &parser_name,
                           uint64_t &ts) {
    if (not line_reader_.consume_and_trim_string(": source_id=") or
        not line_reader_.parse_uint_trim(10, parser_ident)) {
      return false;
    }

    if (!line_reader_.consume_and_trim_string(", source_name=")) {
      return false;
    }
    parser_name =
        line_reader_.extract_and_substr_until(sim_string_utils::is_alnum);
    if (parser_name.empty()) {
      return false;
    }

    if (!line_reader_.consume_and_trim_string(", timestamp=")) {
      return false;
    }
    return line_reader_.parse_uint_trim(10, ts);
  }

  task_t produce(chan_t *tar_chan) override {
    if (not tar_chan) {
      std::cout << "no target channel given" << std::endl;
      co_return;
    }

    if (not line_reader_.open_file(log_file_path_)) {
      std::cout << "could not open log file path" << std::endl;
      co_return;
    }

    while (line_reader_.next_line()) {
      line_reader_.trimL();

      std::function<bool(unsigned char)> pred = [](unsigned char c) {
        return c != ':';
      };
      std::string event_name = line_reader_.extract_and_substr_until(pred);
      if (event_name.empty()) {
        std::cout << "could not parse event name: "
                  << line_reader_.get_raw_line() << std::endl;
        continue;
      }

      uint64_t ts;
      size_t parser_ident;
      std::string parser_name;
      if (not parse_ident_name_ts(parser_ident, parser_name, ts)) {
        std::cout << "could not parse timestamp or source: "
                  << line_reader_.get_raw_line() << std::endl;
        continue;
      }

      /*
       * TODO: get rid of double string comparisons
       */

      event_t event = nullptr;
      uint64_t pc = 0, id = 0, addr = 0, size = 0, vec = 0, dev = 0, func = 0,
               bytes = 0, data = 0, reg = 0, offset = 0, len = 0, intr = 0,
               val = 0, bar = 0;
      std::string function, comp;
      if (event_name.compare("SimSendSyncSimSendSync") == 0) {
        event = std::make_shared<SimSendSync>(ts, parser_ident, parser_name);

      } else if (event_name.compare("SimProcInEvent") == 0) {
        event = std::make_shared<SimProcInEvent>(ts, parser_ident, parser_name);

      } else if (event_name.compare("HostInstr") == 0) {
        if (not line_reader_.consume_and_trim_string(", pc=") or
            not line_reader_.parse_uint_trim(16, pc)) {
          std::cout << "error parsing HostInstr" << std::endl;
          continue;
        }
        event = std::make_shared<HostInstr>(ts, parser_ident, parser_name, pc);

      } else if (event_name.compare("HostCall") == 0) {
        if (not line_reader_.consume_and_trim_string(", pc=") or
            not line_reader_.parse_uint_trim(16, pc) or
            not line_reader_.consume_and_trim_string(", func=") or
            not line_reader_.extract_and_substr_until_into(
                function, sim_string_utils::is_alnum_dot_bar) or
            not line_reader_.consume_and_trim_string(", comp=") or
            not line_reader_.extract_and_substr_until_into(
                comp, sim_string_utils::is_alnum_dot_bar)) {
          std::cout << "error parsing HostInstr" << std::endl;
          continue;
        }
        event = std::make_shared<HostCall>(ts, parser_ident, parser_name, pc,
                                           function, comp);

      } else if (event_name.compare("HostMmioImRespPoW") == 0) {
        event =
            std::make_shared<HostMmioImRespPoW>(ts, parser_ident, parser_name);

      } else if (event_name.compare("HostMmioCR") == 0 or
                 event_name.compare("HostMmioCW") == 0 or
                 event_name.compare("HostDmaC") == 0) {
        if (not line_reader_.consume_and_trim_string(", id=") or
            not line_reader_.parse_uint_trim(10, id)) {
          std::cout << "error parsing HostMmioCR, HostMmioCW or HostDmaC"
                    << std::endl;
          continue;
        }

        if (event_name.compare("HostMmioCR") == 0) {
          event =
              std::make_shared<HostMmioCR>(ts, parser_ident, parser_name, id);
        } else if (event_name.compare("HostMmioCW") == 0) {
          event =
              std::make_shared<HostMmioCW>(ts, parser_ident, parser_name, id);
        } else {
          event = std::make_shared<HostDmaC>(ts, parser_ident, parser_name, id);
        }

      } else if (event_name.compare("HostMmioR") == 0 or
                 event_name.compare("HostMmioW") == 0 or
                 event_name.compare("HostDmaR") == 0 or
                 event_name.compare("HostDmaW") == 0) {
        if (not line_reader_.consume_and_trim_string(", id=") or
            not line_reader_.parse_uint_trim(10, id) or
            not line_reader_.consume_and_trim_string(", addr=") or
            not line_reader_.parse_uint_trim(16, addr) or
            not line_reader_.consume_and_trim_string(", size=") or
            not line_reader_.parse_uint_trim(10, size)) {
          std::cout
              << "error parsing HostMmioR, HostMmioW, HostDmaR or HostDmaW"
              << std::endl;
          continue;
        }

        if (event_name.compare("HostMmioR") == 0 or event_name.compare("HostMmioW") == 0) {
          //if (not line_reader_.consume_and_trim_string(", bar=") or
          //    not line_reader_.parse_uint_trim(10, bar) or
          //    not line_reader_.consume_and_trim_string(", offset=") or
          //    not line_reader_.parse_uint_trim(16, offset) ){
          //  std::cout
          //    << "error parsing HostMmioR, HostMmioW bar or offset"
          //    << std::endl;
          //  continue;
          //}

          // TODO: comment this in!!!!!!!!!!!!!!!!!!!
          bar = 0;
          offset = 0;

          if (event_name.compare("HostMmioW") == 0) {
            event = std::make_shared<HostMmioW>(ts, parser_ident, parser_name, id,
                                              addr, size, bar, offset);
          } else {
            event = std::make_shared<HostMmioR>(ts, parser_ident, parser_name, id,
                                              addr, size, bar, offset);
          }
        } else if (event_name.compare("HostDmaR") == 0) {
          event = std::make_shared<HostDmaR>(ts, parser_ident, parser_name, id,
                                             addr, size);
        } else {
          event = std::make_shared<HostDmaW>(ts, parser_ident, parser_name, id,
                                             addr, size);
        }

      } else if (event_name.compare("HostMsiX") == 0) {
        if (not line_reader_.consume_and_trim_string(", vec=") or
            not line_reader_.parse_uint_trim(10, vec)) {
          std::cout << "error parsing HostMsiX" << std::endl;
          continue;
        }
        event = std::make_shared<HostMsiX>(ts, parser_ident, parser_name, vec);

      } else if (event_name.compare("HostConfRead") == 0 or
                 event_name.compare("HostConfWrite") == 0) {
        if (not line_reader_.consume_and_trim_string(", dev=") or
            not line_reader_.parse_uint_trim(10, dev) or
            not line_reader_.consume_and_trim_string(", func=") or
            not line_reader_.parse_uint_trim(10, func) or
            not line_reader_.consume_and_trim_string(", reg=") or
            not line_reader_.parse_uint_trim(16, reg) or
            not line_reader_.consume_and_trim_string(", bytes=") or
            not line_reader_.parse_uint_trim(10, bytes) or
            not line_reader_.consume_and_trim_string(", data=") or
            not line_reader_.parse_uint_trim(16, data)) {
          std::cout << "error parsing HostConfRead or HostConfWrite"
                    << std::endl;
          continue;
        }

        if (event_name.compare("HostConfRead") == 0) {
          event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                             func, reg, bytes, data, true);
        } else {
          event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                             func, reg, bytes, data, false);
        }

      } else if (event_name.compare("HostClearInt") == 0) {
        event = std::make_shared<HostClearInt>(ts, parser_ident, parser_name);

      } else if (event_name.compare("HostPostInt") == 0) {
        event = std::make_shared<HostPostInt>(ts, parser_ident, parser_name);

      } else if (event_name.compare("HostPciR") == 0 or
                 event_name.compare("HostPciW") == 0) {
        if (not line_reader_.consume_and_trim_string(", offset=") or
            not line_reader_.parse_uint_trim(16, offset) or
            not line_reader_.consume_and_trim_string(", size=") or
            not line_reader_.parse_uint_trim(16, size)) {
          std::cout << "error parsing HostPciR or HostPciW" << std::endl;
          continue;
        }

        if (event_name.compare("HostPciR") == 0) {
          event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                              offset, size, true);
        } else {
          event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                              offset, size, false);
        }

      } else if (event_name.compare("NicMsix") == 0 or
                 event_name.compare("NicMsi") == 0) {
        if (not line_reader_.consume_and_trim_string(", vec=") or
            not line_reader_.parse_uint_trim(10, vec)) {
          std::cout << "error parsing NicMsix" << std::endl;
          continue;
        }

        if (event_name.compare("NicMsix") == 0) {
          event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                            true);
        } else {
          event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                            false);
        }

      } else if (event_name.compare("SetIX") == 0) {
        if (not line_reader_.consume_and_trim_string(", interrupt=") or
            not line_reader_.parse_uint_trim(16, intr)) {
          std::cout << "error parsing NicMsix" << std::endl;
          continue;
        }
        event = std::make_shared<SetIX>(ts, parser_ident, parser_name, intr);

      } else if (event_name.compare("NicDmaI") == 0 or
                 event_name.compare("NicDmaEx") == 0 or
                 event_name.compare("NicDmaEn") == 0 or
                 event_name.compare("NicDmaCR") == 0 or
                 event_name.compare("NicDmaCW") == 0) {
        if (not line_reader_.consume_and_trim_string(", id=") or
            not line_reader_.parse_uint_trim(16, id) or
            not line_reader_.consume_and_trim_string(", addr=") or
            not line_reader_.parse_uint_trim(16, addr) or
            not line_reader_.consume_and_trim_string(", size=") or
            not line_reader_.parse_uint_trim(16, len)) {
          std::cout << "error parsing NicDmaI, NicDmaEx, NicDmaEn, NicDmaCR or "
                       "NicDmaCW"
                    << std::endl;
          continue;
        }

        if (event_name.compare("NicDmaI") == 0) {
          event = std::make_shared<NicDmaI>(ts, parser_ident, parser_name, id,
                                            addr, len);
        } else if (event_name.compare("NicDmaEx") == 0) {
          event = std::make_shared<NicDmaEx>(ts, parser_ident, parser_name, id,
                                             addr, len);
        } else if (event_name.compare("NicDmaEn") == 0) {
          event = std::make_shared<NicDmaEn>(ts, parser_ident, parser_name, id,
                                             addr, len);
        } else if (event_name.compare("NicDmaCW") == 0) {
          event = std::make_shared<NicDmaCR>(ts, parser_ident, parser_name, id,
                                             addr, len);
        } else {
          event = std::make_shared<NicDmaCW>(ts, parser_ident, parser_name, id,
                                             addr, len);
        }

      } else if (event_name.compare("NicMmioR") == 0 or
                 event_name.compare("NicMmioW") == 0) {
        if (not line_reader_.consume_and_trim_string(", off=") or
            not line_reader_.parse_uint_trim(16, offset) or
            not line_reader_.consume_and_trim_string(", len=") or
            not line_reader_.parse_uint_trim(10, len) or
            not line_reader_.consume_and_trim_string(
                ", val=")  // TODO: fix this to ", val=""
            or not line_reader_.parse_uint_trim(16, val)) {
          std::cout << "error parsing NicMmioR or NicMmioW: "
                    << line_reader_.get_raw_line() << std::endl;
          continue;
        }

        if (event_name.compare("NicMmioR") == 0) {
          event = std::make_shared<NicMmioR>(ts, parser_ident, parser_name,
                                             offset, len, val);
        } else {
          event = std::make_shared<NicMmioW>(ts, parser_ident, parser_name,
                                             offset, len, val);
        }

      } else if (event_name.compare("NicTx") == 0) {
        if (not line_reader_.consume_and_trim_string(", len=") or
            not line_reader_.parse_uint_trim(10, len)) {
          std::cout << "error parsing NicTx" << std::endl;
          continue;
        }
        event = std::make_shared<NicTx>(ts, parser_ident, parser_name, len);

      } else if (event_name.compare("NicRx") == 0) {
        if (not line_reader_.consume_and_trim_string(", len=") or
            not line_reader_.parse_uint_trim(10, len) or
            not line_reader_.consume_and_trim_string(", port=") or
            not line_reader_.parse_uint_trim(10, addr)) {
          std::cout << "error parsing NicRx" << std::endl;
          continue;
        }
        event =
            std::make_shared<NicRx>(ts, parser_ident, parser_name, len, addr);

      } else {
        std::cout << "unknown event found, it will be skipped" << std::endl;
        continue;
      }

      if (not event) {
        std::cout << "no event to write, there should be one" << std::endl;
        co_return;
      }

      if (not co_await tar_chan->write(event)) {
        std::cout << "could not write event to the target channel" << std::endl;
        co_return;
      }
    }

    co_return;
  };

  explicit event_stream_parser(const std::string log_file_path,
                               LineReader &line_reader)
      : sim::coroutine::producer<event_t>(),
        log_file_path_(std::move(log_file_path)),
        line_reader_(line_reader) {
  }

 private:
  const std::string log_file_path_;
  LineReader &line_reader_;
};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_
