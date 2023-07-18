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

#include "util/exception.h"
#include "corobelt/corobelt.h"
#include "reader/reader.h"
#include "util/string_util.h"
#include "env/traceEnvironment.h"

#ifndef SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_
#define SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_


struct EventStreamParser : public producer<std::shared_ptr<Event>> {
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

  concurrencpp::result<void> produce(std::shared_ptr<concurrencpp::executor> resume_executor,
                                     std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override {
    throw_if_empty(resume_executor, resume_executor_null);
    throw_if_empty(tar_chan, channel_is_null);

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

      std::shared_ptr<Event> event = nullptr;
      auto &l_reader = line_reader_;
      uint64_t pc = 0, id = 0, addr = 0, size = 0, vec = 0, dev = 0, func = 0,
               bytes = 0, data = 0, reg = 0, offset = 0, len = 0, intr = 0,
               val = 0, bar = 0;
      std::string function, component;
      if (event_name == "SimSendSyncSimSendSync") {
        event = std::make_shared<SimSendSync>(ts, parser_ident, parser_name);

      } else if (event_name == "SimProcInEvent") {
        event = std::make_shared<SimProcInEvent>(ts, parser_ident, parser_name);

      } else if (event_name == "HostInstr") {
        if (not line_reader_.consume_and_trim_string(", pc=") or
            not line_reader_.parse_uint_trim(16, pc)) {
          std::cout << "error parsing HostInstr" << std::endl;
          continue;
        }
        event = std::make_shared<HostInstr>(ts, parser_ident, parser_name, pc);

      } else if (event_name == "HostCall") {
        if (not line_reader_.consume_and_trim_string(", pc=") or
            not line_reader_.parse_uint_trim(16, pc) or
            not line_reader_.consume_and_trim_string(", func=") or
            not line_reader_.extract_and_substr_until_into(
                function, sim_string_utils::is_alnum_dot_bar) or
            not line_reader_.consume_and_trim_string(", comp=") or
            not line_reader_.extract_and_substr_until_into(
                component, sim_string_utils::is_alnum_dot_bar)) {
          std::cout << "error parsing HostInstr" << std::endl;
          continue;
        }
        const std::string *func =
            TraceEnvironment::internalize_additional(function);
        const std::string *comp =
            TraceEnvironment::internalize_additional(component);

        event = std::make_shared<HostCall>(ts, parser_ident, parser_name, pc,
                                           func, comp);

      } else if (event_name == "HostMmioImRespPoW") {
        event =
            std::make_shared<HostMmioImRespPoW>(ts, parser_ident, parser_name);

      } else if (event_name == "HostMmioCR" or
                 event_name == "HostMmioCW" or
                 event_name == "HostDmaC") {
        if (not line_reader_.consume_and_trim_string(", id=") or
            not line_reader_.parse_uint_trim(10, id)) {
          std::cout << "error parsing HostMmioCR, HostMmioCW or HostDmaC"
                    << std::endl;
          continue;
        }

        if (event_name == "HostMmioCR") {
          event =
              std::make_shared<HostMmioCR>(ts, parser_ident, parser_name, id);
        } else if (event_name == "HostMmioCW") {
          event =
              std::make_shared<HostMmioCW>(ts, parser_ident, parser_name, id);
        } else {
          event = std::make_shared<HostDmaC>(ts, parser_ident, parser_name, id);
        }

      } else if (event_name == "HostMmioR" or
                 event_name == "HostMmioW" or
                 event_name == "HostDmaR" or
                 event_name == "HostDmaW") {
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

        if (event_name == "HostMmioR" or
            event_name == "HostMmioW") {
          // if (not line_reader_.consume_and_trim_string(", bar=") or
          //     not line_reader_.parse_uint_trim(10, bar) or
          //     not line_reader_.consume_and_trim_string(", offset=") or
          //     not line_reader_.parse_uint_trim(16, offset) ){
          //   std::cout
          //     << "error parsing HostMmioR, HostMmioW bar or offset"
          //     << std::endl;
          //   continue;
          // }

          // TODO: comment this in!!!!!!!!!!!!!!!!!!!
          bar = 0;
          offset = 0;

          if (event_name == "HostMmioW") {
            event = std::make_shared<HostMmioW>(ts, parser_ident, parser_name,
                                                id, addr, size, bar, offset);
          } else {
            event = std::make_shared<HostMmioR>(ts, parser_ident, parser_name,
                                                id, addr, size, bar, offset);
          }
        } else if (event_name == "HostDmaR") {
          event = std::make_shared<HostDmaR>(ts, parser_ident, parser_name, id,
                                             addr, size);
        } else {
          event = std::make_shared<HostDmaW>(ts, parser_ident, parser_name, id,
                                             addr, size);
        }

      } else if (event_name == "HostMsiX") {
        if (not line_reader_.consume_and_trim_string(", vec=") or
            not line_reader_.parse_uint_trim(10, vec)) {
          std::cout << "error parsing HostMsiX" << std::endl;
          continue;
        }
        event = std::make_shared<HostMsiX>(ts, parser_ident, parser_name, vec);

      } else if (event_name == "HostConfRead" or
                 event_name == "HostConfWrite") {
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

        if (event_name == "HostConfRead") {
          event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                             func, reg, bytes, data, true);
        } else {
          event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                             func, reg, bytes, data, false);
        }

      } else if (event_name == "HostClearInt") {
        event = std::make_shared<HostClearInt>(ts, parser_ident, parser_name);

      } else if (event_name == "HostPostInt") {
        event = std::make_shared<HostPostInt>(ts, parser_ident, parser_name);

      } else if (event_name == "HostPciR" or
                 event_name == "HostPciW") {
        if (not line_reader_.consume_and_trim_string(", offset=") or
            not line_reader_.parse_uint_trim(16, offset) or
            not line_reader_.consume_and_trim_string(", size=") or
            not line_reader_.parse_uint_trim(16, size)) {
          std::cout << "error parsing HostPciR or HostPciW" << std::endl;
          continue;
        }

        if (event_name == "HostPciR") {
          event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                              offset, size, true);
        } else {
          event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                              offset, size, false);
        }

      } else if (event_name == "NicMsix" or
                 event_name == "NicMsi") {
        if (not line_reader_.consume_and_trim_string(", vec=") or
            not line_reader_.parse_uint_trim(10, vec)) {
          std::cout << "error parsing NicMsix" << std::endl;
          continue;
        }

        if (event_name == "NicMsix") {
          event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                            true);
        } else {
          event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                            false);
        }

      } else if (event_name == "SetIX") {
        if (not line_reader_.consume_and_trim_string(", interrupt=") or
            not line_reader_.parse_uint_trim(16, intr)) {
          std::cout << "error parsing NicMsix" << std::endl;
          continue;
        }
        event = std::make_shared<SetIX>(ts, parser_ident, parser_name, intr);

      } else if (event_name == "NicDmaI" or
                 event_name == "NicDmaEx" or
                 event_name == "NicDmaEn" or
                 event_name == "NicDmaCR" or
                 event_name == "NicDmaCW") {
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

        if (event_name == "NicDmaI") {
          event = std::make_shared<NicDmaI>(ts, parser_ident, parser_name, id,
                                            addr, len);
        } else if (event_name == "NicDmaEx") {
          event = std::make_shared<NicDmaEx>(ts, parser_ident, parser_name, id,
                                             addr, len);
        } else if (event_name == "NicDmaEn") {
          event = std::make_shared<NicDmaEn>(ts, parser_ident, parser_name, id,
                                             addr, len);
        } else if (event_name == "NicDmaCW") {
          event = std::make_shared<NicDmaCW>(ts, parser_ident, parser_name, id,
                                             addr, len);
        } else {
          event = std::make_shared<NicDmaCR>(ts, parser_ident, parser_name, id,
                                             addr, len);
        }

      } else if (event_name == "NicMmioR" or
                 event_name == "NicMmioW") {
        if (not line_reader_.consume_and_trim_string(", off=") or
            not line_reader_.parse_uint_trim(16, offset) or
            not line_reader_.consume_and_trim_string(", len=") or
            not line_reader_.parse_uint_trim(10, len) or
            not line_reader_.consume_and_trim_string(
                ", val=")
            or not line_reader_.parse_uint_trim(16, val)) {
          std::cout << "error parsing NicMmioR or NicMmioW: "
                    << line_reader_.get_raw_line() << std::endl;
          continue;
        }

        if (event_name == "NicMmioR") {
          event = std::make_shared<NicMmioR>(ts, parser_ident, parser_name,
                                             offset, len, val);
        } else {
          event = std::make_shared<NicMmioW>(ts, parser_ident, parser_name,
                                             offset, len, val);
        }

      } else if (event_name == "NicTx") {
        if (not line_reader_.consume_and_trim_string(", len=") or
            not line_reader_.parse_uint_trim(10, len)) {
          std::cout << "error parsing NicTx" << std::endl;
          continue;
        }
        event = std::make_shared<NicTx>(ts, parser_ident, parser_name, len);

      } else if (event_name == "NicRx") {
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

      throw_if_empty(event, event_is_null);
      co_await tar_chan->Push(resume_executor, event);
    }

    co_return;
  };

  static auto create(const std::string log_file_path,
                                                   LineReader &line_reader) {
    auto parser = std::make_shared<EventStreamParser>(log_file_path, line_reader);
    throw_if_empty(parser, event_stream_parser_null);
    return parser;
  }

  explicit EventStreamParser(const std::string log_file_path,
                               LineReader &line_reader)
      : producer<std::shared_ptr<Event>>(),
        log_file_path_(log_file_path),
        line_reader_(line_reader) {
  }

 private:
  const std::string log_file_path_;
  LineReader &line_reader_;
};

#endif  // SIMBRICKS_TRACE_EVENT_STREAM_PARSER_H_
