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
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "corobelt/corobelt.h"
#include "env/symtable.h"
#include "env/traceEnvironment.h"
#include "events/event-filter.h"
#include "events/eventStreamParser.h"
#include "events/events.h"
#include "parser/parser.h"
#include "reader/reader.h"
#include "analytics/spanner.h"
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

std::shared_ptr<EventPrinter> createPrinter(cxxopts::ParseResult &result, const std::string &option) {
  std::shared_ptr<EventPrinter> printer;
  if (result.count(option) != 0) {
    std::ofstream out_file;
    if (not create_open_file(out_file, result[option].as<std::string>())) {
      std::cerr << "could not open gem5-client-events file" << std::endl;
      return nullptr;
    }
    printer = EventPrinter::create(out_file);
  } else {
    printer = EventPrinter::create(std::cout);
  }

  return printer;
}

int main(int argc, char *argv[]) {

  cxxopts::Options options("trace", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")
      ("linux-dump-server-client", "file path to a output file obtained by 'objdump -S linux_image'", cxxopts::value<std::string>())
      ("nic-i40e-dump","file path to a output file obtained by 'objdump -d i40e.ko' (driver)",cxxopts::value<std::string>())
      ("gem5-log-server", "file path to a server log file written by gem5",cxxopts::value<std::string>())
      ("gem5-server-events","file to which the server event stream is written to", cxxopts::value<std::string>())
      ("nicbm-log-server", "file path to a server log file written by the nicbm",cxxopts::value<std::string>())
      ("nicbm-server-events", "file to which the server nic event stream is written to", cxxopts::value<std::string>())
      ("gem5-log-client", "file path to a client log file written by gem5", cxxopts::value<std::string>())
      ("gem5-client-events", "file to which the client event stream is written to", cxxopts::value<std::string>())
      ("nicbm-log-client", "file path to a client log file written by the nicbm", cxxopts::value<std::string>())
      ("nicbm-client-events", "file to which the client nic event stream is written to", cxxopts::value<std::string>())
      ("ts-lower-bound", "lower timestamp bound for events", cxxopts::value<std::string>())
      ("ts-upper-bound", "upper timestamp bound for events", cxxopts::value<std::string>())
      ("event-stream-log", "file path to file that stores an event stream", cxxopts::value<std::string>())
      ("gem5-server-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("gem5-client-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("nicbm-server-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("nicbm-client-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ;

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
  // Init runtime and set threads to use --> IMPORTANT
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 4;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  if (result.count("gem5-server-event-stream") and result.count("gem5-client-event-stream")
      and result.count("nicbm-server-event-stream") and result.count("nicbm-client-event-stream")) {

    Tracer tracer;

    ContextQueue client_hn;
    ContextQueue server_client_nn;
    ContextQueue server_hn;

    auto server_host_task = [&]() {
      LineReader lr;
      auto parser = EventStreamParser::create(result["gem5-server-event-stream"].as<std::string>(), lr);

      auto spanner = HostSpanner::create(tracer, server_hn, false);

      //run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, parser, spanner);
    };

    auto client_host_task = [&]() {
      LineReader lr;
      auto parser = EventStreamParser::create(result["gem5-client-event-stream"].as<std::string>(), lr);

      auto spanner = HostSpanner::create(tracer, client_hn, true);

      //run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, parser, spanner);
    };

    auto server_nic_task = [&]() {
      LineReader lr;
      auto parser = EventStreamParser::create(result["nicbm-server-event-stream"].as<std::string>(), lr);

      auto spanner = NicSpanner::create(tracer, server_hn, server_client_nn);

      //run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, parser, spanner);
    };

    auto client_nic_task = [&]() {
      LineReader lr;
      auto parser = EventStreamParser::create(result["nicbm-client-event-stream"].as<std::string>(), lr);

      auto spanner = NicSpanner::create(tracer, client_hn, server_client_nn);

      //run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, parser, spanner);
    };

    client_host_task();
    client_nic_task();
    server_nic_task();
    server_host_task();

    exit(EXIT_SUCCESS);
  }

  if (!result.count("linux-dump-server-client") ||
      !result.count("gem5-log-server") ||
      !result.count("nicbm-log-server") ||
      !result.count("gem5-log-client") ||
      !result.count("nicbm-log-client")) {
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
          FilterType::S)) {
    std::cerr << "could not initialize symbol table linux-dump-server-client"
              << std::endl;
    exit(EXIT_FAILURE);
  }
  if (result.count("nic-i40e-dump") &&
      !trace_environment::add_symbol_table(
          "Nicdriver-Symbols", result["nic-i40e-dump"].as<std::string>(),
          0xffffffffa0000000ULL, FilterType::S)) {
    std::cerr << "could not initialize symbol table nic-i40e-dump" << std::endl;
    exit(EXIT_FAILURE);
  }

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

  auto server_host_task = [&]() {  // SERVER HOST PIPELINE
    std::set<EventType> to_filter{EventType::HostInstr_t, EventType::SimProcInEvent_t, EventType::SimSendSync_t};
    auto event_filter = std::make_shared<EventTypeFilter>(to_filter, true);

    std::vector<EventTimestampFilter::EventTimeBoundary> bounds{
      EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};
    auto timestamp_filter = EventTimestampFilter::create(bounds);

    ComponentFilter comp_filter_server("ComponentFilter-Server");
    LineReader server_lr;
    auto gem5_server_par = Gem5Parser::create("Gem5ServerParser",
                                                              result["gem5-log-server"].as<std::string>(),
                                                              comp_filter_server, server_lr);{};

    auto printer = createPrinter(result, "gem5-server-events");
    if (not printer) {
      exit(EXIT_FAILURE);
    }

    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipes{timestamp_filter, event_filter};
    run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, gem5_server_par, pipes, printer);
  };

  auto client_host_task = [&]() {  // CLIENT HOST PIPELINE
    std::set<EventType> to_filter{EventType::HostInstr_t, EventType::SimProcInEvent_t,
                                     EventType::SimSendSync_t};
    auto event_filter = EventTypeFilter::create(to_filter, true);

    std::vector<EventTimestampFilter::EventTimeBoundary> bounds{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};
    auto timestamp_filter = EventTimestampFilter::create(bounds);

    ComponentFilter comp_filter_client("ComponentFilter-Server");
    LineReader client_lr;
    auto gem5_client_par = Gem5Parser::create("Gem5ClientParser",
                             result["gem5-log-client"].as<std::string>(),
                             comp_filter_client, client_lr);

    auto printer = createPrinter(result, "gem5-client-events");
    if (not printer) {
      exit(EXIT_FAILURE);
    }

    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipes{timestamp_filter, event_filter};
    run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, gem5_client_par, pipes, printer);
  };

  auto server_nic_task = [&]() {  // SERVER NIC PIPELINE
    std::set<EventType> to_filter{EventType::SimProcInEvent_t, EventType::SimSendSync_t};
    auto event_filter = EventTypeFilter::create(to_filter, true);

    std::vector<EventTimestampFilter::EventTimeBoundary> bounds{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};
    auto timestamp_filter = EventTimestampFilter::create(bounds);

    LineReader nic_ser_lr;
    auto nic_ser_par = NicBmParser::create("NicbmServerParser",
                          result["nicbm-log-server"].as<std::string>(), nic_ser_lr);

    auto printer = createPrinter(result, "nicbm-server-events");
    if (not printer) {
      exit(EXIT_FAILURE);
    }

    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipes{timestamp_filter, event_filter};
    run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, nic_ser_par, pipes, printer);
  };

  auto client_nic_task = [&]() {  // CLIENT NIC PIPELINE
    std::set<EventType> to_filter{EventType::SimProcInEvent_t, EventType::SimSendSync_t};
    auto event_filter = EventTypeFilter::create(to_filter, true);

    std::vector<EventTimestampFilter::EventTimeBoundary> bounds{
        EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};
    auto timestamp_filter = EventTimestampFilter::create(bounds);

    LineReader nic_cli_lr;
    auto nic_cli_par = NicBmParser::create("NicbmClientParser",
                          result["nicbm-log-client"].as<std::string>(), nic_cli_lr);

    auto printer = createPrinter(result, "nicbm-client-events");
    if (not printer) {
      exit(EXIT_FAILURE);
    }

    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipes{timestamp_filter, event_filter};
    run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, nic_cli_par, pipes, printer);
  };

  try {
    server_host_task();
    client_host_task();
    server_nic_task();
    client_nic_task();
  } catch (const std::runtime_error& err) {
    std::cerr << err.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
