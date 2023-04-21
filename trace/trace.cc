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
#include <set>
#include <string>

#include "lib/utils/cxxopts.hpp"
#include "lib/utils/log.h"
#include "trace/analytics/packer.h"
#include "trace/corobelt/corobelt.h"
#include "trace/env/symtable.h"
#include "trace/env/traceEnvironment.h"
#include "trace/events/event-filter.h"
#include "trace/events/eventStreamParser.h"
#include "trace/events/events.h"
#include "trace/parser/parser.h"

int main(int argc, char *argv[]) {
  using event_t = std::shared_ptr<Event>;
  using pack_t = std::shared_ptr<event_pack>;
  // using trace_t = std::shared_ptr<tcp_trace>;

  cxxopts::Options options("trace", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")(
      "linux-dump-server-client",
      "file path to a output file obtained by 'objdump -S linux_image'",
      cxxopts::value<std::string>())(
      "nic-i40e-dump",
      "file path to a output file obtained by 'objdump -d i40e.ko' (driver)",
      cxxopts::value<std::string>())(
      "gem5-log-server", "file path to a server log file written by gem5",
      cxxopts::value<std::string>())(
      "nicbm-log-server", "file path to a server log file written by the nicbm",
      cxxopts::value<std::string>())(
      "gem5-log-client", "file path to a client log file written by gem5",
      cxxopts::value<std::string>())(
      "nicbm-log-client", "file path to a client log file written by the nicbm",
      cxxopts::value<std::string>())("ts-lower-bound",
                                     "lower timestamp bound for events",
                                     cxxopts::value<std::string>())(
      "ts-upper-bound", "upper timestamp bound for events",
      cxxopts::value<std::string>())(
      "event-stream-log", "file path to file that stores an event stream",
      cxxopts::value<std::string>());

  cxxopts::ParseResult result;
  try {
    result = options.parse(argc, argv);

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(EXIT_SUCCESS);
  }

  sim::trace::env::trace_environment env;

  // printer to consume pipeline and to print events
  EventPrinter eventPrinter(std::cout);

  if (result.count("event-stream-log")) {
    LineReader streamLr;
    event_stream_parser streamParser(
        result["event-stream-log"].as<std::string>(), streamLr, env);

    pack_printer p_printer;
    EventTypeFilter filter{
        {// EventType::HostCall_t, 
         // EventType::HostMmioW_t,
         // EventType::HostMmioR_t,
         // EventType::HostMmioImRespPoW_t,
         // EventType::HostMmioCW_t,
         // EventType::HostMmioCR_t,
         EventType::HostDmaW_t, 
         EventType::HostDmaR_t, 
         EventType::HostDmaC_t,
         // EventType::NicMmioW_t,
         // EventType::NicMmioR_t,
         // EventType::NicDmaI_t,
         // EventType::NicDmaEx_t,
         // EventType::NicDmaCW_t,
         // EventType::NicDmaCR_t,
         // EventType::NicTx_t,
         // EventType::NicRx_t,
         // EventType::NicMsix_t,
         // EventType::HostMsiX_t, 
         // EventType::HostPostInt_t,
         // EventType::HostClearInt_t
         }};
    sim::corobelt::pipeline<event_t> pipel{streamParser, {filter}};
    host_packer packer{pipel, env};
    //nic_packer packer{pipel, env};

    if (!sim::corobelt::awaiter<pack_t>::await_termination(packer,
                                                            p_printer)) {
      std::cerr << "could not await termination of the pipeline" << std::endl;
      exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
  }

  if (!result.count("linux-dump-server-client") ||
      !result.count("gem5-log-server") || !result.count("nicbm-log-server") ||
      !result.count("gem5-log-client") || !result.count("nicbm-log-client")) {
    std::cerr << "invalid arguments given" << std::endl
              << options.help() << std::endl;
    exit(EXIT_FAILURE);
  }

  // symbol filter to translate hex address to function-name/label
  if (result.count("linux-dump-server-client") &&
       !env.add_symbol_table(
           "Linuxvm-Symbols",
           result["linux-dump-server-client"].as<std::string>(), 0,
           FilterType::S)) {
    std::cerr << "could not initialize symbol table linux-dump-server-client" << std::endl;
    exit(EXIT_FAILURE);
  }
  
  if (result.count("nic-i40e-dump") &&
       !env.add_symbol_table("Nicdriver-Symbols",
                             result["nic-i40e-dump"].as<std::string>(),
                             0xffffffffa0000000ULL, FilterType::S)) {
    std::cerr << "could not initialize symbol table nic-i40e-dump" << std::endl;
    exit(EXIT_FAILURE);
  }

  /*
   * Parsers
   */

  // component filter to only parse events written from certain components
  ComponentFilter compF("ComponentFilter-Client-Server");

  // gem5 log parser that generates events
  LineReader serverLr;
  Gem5Parser gem5ServerPar{"Gem5ServerParser",
                           result["gem5-log-server"].as<std::string>(), env,
                           compF, serverLr};

  LineReader clientLr;
  Gem5Parser gem5ClientPar{"Gem5ClientParser",
                           result["gem5-log-client"].as<std::string>(), env,
                           compF, clientLr};

  // nicbm log parser that generates events
  LineReader nicSerLr;
  NicBmParser nicSerPar{"NicbmServerParser",
                        result["nicbm-log-server"].as<std::string>(), nicSerLr,
                        env};
  LineReader nicCliLr;
  NicBmParser nicCliPar{"NicbmClientParser",
                        result["nicbm-log-client"].as<std::string>(), nicCliLr,
                        env};

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // TODO:
  // write an string internalizer for function and component names and
  // thereof... this will enable string singeltons and hence comparison through
  // pointer comparison to check for specific function boundaries etc.
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  /*
   * Filter and operators
   */
  // 1475802058125
  // 1596059510250
  uint64_t lower_bound =
      EventTimestampFilter::EventTimeBoundary::MIN_LOWER_BOUND;
  uint64_t upper_bound =
      EventTimestampFilter::EventTimeBoundary::MAX_UPPER_BOUND;

  if (result.count("ts-upper-bound"))
    sim_string_utils::parse_uint_trim_copy(
        result["ts-upper-bound"].as<std::string>(), 10, &upper_bound);

  if (result.count("ts-lower-bound"))
    sim_string_utils::parse_uint_trim_copy(
        result["ts-lower-bound"].as<std::string>(), 10, &lower_bound);

  EventTimestampFilter timestampFilter{
      EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

  // filter uninteresting events out of stream
  EventTypeFilter eventFilter{
      {EventType::HostInstr_t, EventType::SimProcInEvent_t,
       EventType::SimSendSync_t},
      true};

  using event_t = std::shared_ptr<Event>;

  sim::corobelt::collector<event_t, EventComperator> collector{
      {nicCliPar, gem5ClientPar, nicSerPar, gem5ServerPar}};

  sim::corobelt::pipeline<event_t> pipeline{collector,
                                            {timestampFilter, eventFilter}};

  if (!sim::corobelt::awaiter<event_t>::await_termination(pipeline,
                                                          eventPrinter)) {
    std::cerr << "could not await termination of the pipeline" << std::endl;
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
