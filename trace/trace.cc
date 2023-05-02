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

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <thread>

#include "packer.h"
#include "corobelt/corobelt.h"
#include "env/symtable.h"
#include "env/traceEnvironment.h"
#include "events/event-filter.h"
#include "events/eventStreamParser.h"
#include "events/events.h"
#include "parser/parser.h"
#include "util/cxxopts.hpp"
#include "util/log.h"

bool create_open_file(std::ofstream &new_out, std::string filename) {
  try {
    // TODO: later let user specify output directory 
    // auto path = std::filesystem::path(directory);
    // auto targetPath = std::filesystem::canonical(path);
    if (std::filesystem::exists(filename)) {
      std::cerr << "the file " << filename
                << " already exists, we will not overwrite it" << std::endl;
      return false;
    }

    new_out.open(filename, std::ios::out);
    return new_out.is_open();

  } catch (const std::exception &ex) {
    std::cerr << "exception opening file " << ex.what() << std::endl;
    return false;
  }

  return true;
}

int main(int argc, char *argv[]) {
  using event_t = std::shared_ptr<Event>;

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
      "gem5-server-events",
      "file to which the server event stream is written to",
      cxxopts::value<std::string>())(
      "nicbm-log-server", "file path to a server log file written by the nicbm",
      cxxopts::value<std::string>())(
      "nicbm-server-events",
      "file to which the server nic event stream is written to",
      cxxopts::value<std::string>())(
      "gem5-log-client", "file path to a client log file written by gem5",
      cxxopts::value<std::string>())(
      "gem5-client-events",
      "file to which the client event stream is written to",
      cxxopts::value<std::string>())(
      "nicbm-log-client", "file path to a client log file written by the nicbm",
      cxxopts::value<std::string>())(
      "nicbm-client-events",
      "file to which the client nic event stream is written to",
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

  // Init the trace environment --> IMPORTANT
  trace_environment::initialize();

#if 0
  if (result.count("event-stream-log")) {
    LineReader streamLr;
    event_stream_parser streamParser(
        result["event-stream-log"].as<std::string>(), streamLr);

    pack_printer p_printer;
    EventTypeFilter filter{{
        // EventType::HostCall_t,
        // EventType::HostMmioW_t,
        // EventType::HostMmioR_t,
        // EventType::HostMmioImRespPoW_t,
        // EventType::HostMmioCW_t,
        // EventType::HostMmioCR_t,
        EventType::HostDmaW_t, EventType::HostDmaR_t, EventType::HostDmaC_t,
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
    host_packer packer{pipel};
    // nic_packer packer{pipel, env};

    if (!sim::corobelt::awaiter<pack_t>::await_termination(packer, p_printer)) {
      std::cerr << "could not await termination of the pipeline" << std::endl;
      exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
  }
#endif

  if (!result.count("linux-dump-server-client") ||
      !result.count("gem5-log-server") || !result.count("gem5-server-events") ||
      !result.count("nicbm-log-server") ||
      !result.count("nicbm-server-events") ||
      !result.count("gem5-log-client") || !result.count("gem5-client-events") ||
      !result.count("nicbm-log-client") ||
      !result.count("nicbm-client-events")) {
    std::cerr << "invalid arguments given" << std::endl
              << options.help() << std::endl;
    exit(EXIT_FAILURE);
  }

  // add both symbol tables / symbol filterr to translate hex addresses to
  // function-name/label
  if (result.count("linux-dump-server-client") &&
      !trace_environment::add_symbol_table(
          "Linuxvm-Symbols",
          result["linux-dump-server-client"].as<std::string>(), 0,
          FilterType::Elf)) {
    std::cerr << "could not initialize symbol table linux-dump-server-client"
              << std::endl;
    exit(EXIT_FAILURE);
  }
  if (result.count("nic-i40e-dump") &&
      !trace_environment::add_symbol_table(
          "Nicdriver-Symbols", result["nic-i40e-dump"].as<std::string>(),
          0xffffffffa0000000ULL, FilterType::Elf)) {
    std::cerr << "could not initialize symbol table nic-i40e-dump" << std::endl;
    exit(EXIT_FAILURE);
  }

  // 1475802058125
  // 1596059510250
  uint64_t lower_bound =
      EventTimestampFilter::EventTimeBoundary::MIN_LOWER_BOUND;
  uint64_t upper_bound =
      EventTimestampFilter::EventTimeBoundary::MAX_UPPER_BOUND;

  if (result.count("ts-upper-bound")) {
    sim_string_utils::parse_uint_trim_copy(
        result["ts-upper-bound"].as<std::string>(), 10, &upper_bound);
  }

  if (result.count("ts-lower-bound")) {
    sim_string_utils::parse_uint_trim_copy(
        result["ts-lower-bound"].as<std::string>(), 10, &lower_bound);
  }

  
  auto server_host_task = [&](){  // SERVER HOST PIPELINE
    EventTypeFilter eventFilter{
        {EventType::HostInstr_t, EventType::SimProcInEvent_t,
         EventType::SimSendSync_t},
        true};

    EventTimestampFilter timestampFilter{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

    ComponentFilter compFilterServer("ComponentFilter-Server");
    LineReader serverLr;
    Gem5Parser gem5ServerPar{"Gem5ServerParser",
                             result["gem5-log-server"].as<std::string>(),
                             compFilterServer, serverLr};

    std::ofstream out_file;
    if (not create_open_file(out_file,
                             result["gem5-server-events"].as<std::string>())) {
      std::cerr << "could not open gem5-server-events file" << std::endl;
      exit(EXIT_FAILURE);
    }
    EventPrinter printer{out_file};

    sim::corobelt::pipeline<std::shared_ptr<Event>> pipeline{
        gem5ServerPar,
        {
            timestampFilter,
            eventFilter,
        }};

    if (!sim::corobelt::awaiter<event_t>::await_termination(pipeline,
                                                            printer)) {
      std::cerr << "could not await termination of the pipeline" << std::endl;
      exit(EXIT_FAILURE);
    }
  };

  auto client_host_task = [&](){  // CLIENT HOST PIPELINE
    EventTypeFilter eventFilter{
        {EventType::HostInstr_t, EventType::SimProcInEvent_t,
         EventType::SimSendSync_t},
        true};

    EventTimestampFilter timestampFilter{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

    ComponentFilter compFilterClient("ComponentFilter-Server");
    LineReader clientLr;
    Gem5Parser gem5ClientPar{"Gem5ClientParser",
                             result["gem5-log-client"].as<std::string>(),
                             compFilterClient, clientLr};

    std::ofstream out_file;
    if (not create_open_file(out_file,
                             result["gem5-client-events"].as<std::string>())) {
      std::cerr << "could not open gem5-client-events file" << std::endl;
      exit(EXIT_FAILURE);
    }
    EventPrinter printer{out_file};

    sim::corobelt::pipeline<std::shared_ptr<Event>> pipeline{
        gem5ClientPar, {timestampFilter, eventFilter}};

    if (!sim::corobelt::awaiter<event_t>::await_termination(pipeline,
                                                            printer)) {
      std::cerr << "could not await termination of the pipeline" << std::endl;
      exit(EXIT_FAILURE);
    }
  };

  auto server_nic_task = [&]() {  // SERVER NIC PIPELINE
    EventTypeFilter eventFilter{
        {EventType::SimProcInEvent_t, EventType::SimSendSync_t}, true};

    EventTimestampFilter timestampFilter{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

    LineReader nicSerLr;
    NicBmParser nicSerPar{"NicbmServerParser",
                          result["nicbm-log-server"].as<std::string>(),
                          nicSerLr};

    std::ofstream out_file;
    if (not create_open_file(out_file,
                             result["nicbm-server-events"].as<std::string>())) {
      std::cerr << "could not open nicbm-server-events file" << std::endl;
      exit(EXIT_FAILURE);
    }
    EventPrinter printer{out_file};

    sim::corobelt::pipeline<std::shared_ptr<Event>> pipeline{
        nicSerPar, {timestampFilter, eventFilter}};

    if (!sim::corobelt::awaiter<event_t>::await_termination(pipeline,
                                                            printer)) {
      std::cerr << "could not await termination of the pipeline" << std::endl;
      exit(EXIT_FAILURE);
    }
  };

  auto client_nic_task = [&]() {  // CLIENT NIC PIPELINE
    EventTypeFilter eventFilter{
        {EventType::SimProcInEvent_t, EventType::SimSendSync_t}, true};

    EventTimestampFilter timestampFilter{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

    LineReader nicCliLr;
    NicBmParser nicCliPar{"NicbmClientParser",
                          result["nicbm-log-client"].as<std::string>(),
                          nicCliLr};

    std::ofstream out_file;
    if (not create_open_file(out_file,
                             result["nicbm-client-events"].as<std::string>())) {
      std::cerr << "could not open nicbm-client-events file" << std::endl;
      exit(EXIT_FAILURE);
    }
    EventPrinter printer{out_file};

    sim::corobelt::pipeline<std::shared_ptr<Event>> pipeline{
        nicCliPar, {timestampFilter, eventFilter}};

    if (!sim::corobelt::awaiter<event_t>::await_termination(pipeline,
                                                            printer)) {
      std::cerr << "could not await termination of the pipeline" << std::endl;
      exit(EXIT_FAILURE);
    }
  };

  std::thread server_h(server_host_task);
  std::thread client_h(client_host_task);
  std::thread server_n(server_nic_task);
  std::thread client_n(client_nic_task);

  server_h.join();
  client_h.join();
  server_n.join();
  client_n.join();

  exit(EXIT_SUCCESS);
}
