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
#include "util/factory.h"
#include "exporter/exporter.h"

void create_open_file(std::ofstream &out, const std::string &filename, bool allow_override) {
  if (not allow_override and std::filesystem::exists(filename)) {
    std::stringstream error;
    error << "the file " << filename << " already exists, we will not overwrite it";
    throw std::runtime_error(error.str());
  }

  out.open(filename, std::ios::out);
  if (not out.is_open()) {
    std::stringstream error;
    error << "could not open file " << filename;
    throw std::runtime_error(error.str());
  }
}

std::shared_ptr<EventPrinter> createPrinter(std::ofstream &out,
                                            cxxopts::ParseResult &result,
                                            const std::string &option,
                                            bool allow_override) {
  std::shared_ptr<EventPrinter> printer;
  if (result.count(option) != 0) {
    try {
      create_open_file(out, result[option].as<std::string>(), allow_override);
      printer = create_shared<EventPrinter>(printer_is_null, out);
    } catch (std::exception &exe) {
      std::cerr << "could not create printer: " << exe.what() << std::endl;
      return nullptr;
    }
  } else {
    printer = create_shared<EventPrinter>(printer_is_null, std::cout);
  }

  return printer;
}

int main(int argc, char *argv[]) {

  cxxopts::Options options("trace", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")
      ("linux-dump-server-client",
       "file path to a output file obtained by 'objdump -S linux_image'",
       cxxopts::value<std::string>())
      ("nic-i40e-dump",
       "file path to a output file obtained by 'objdump -d i40e.ko' (driver)",
       cxxopts::value<std::string>())
      ("gem5-log-server", "file path to a server log file written by gem5", cxxopts::value<std::string>())
      ("gem5-server-events", "file to which the server event stream is written to", cxxopts::value<std::string>())
      ("nicbm-log-server", "file path to a server log file written by the nicbm", cxxopts::value<std::string>())
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
      ("nicbm-client-event-stream", "create trace by using the event stream", cxxopts::value<std::string>());

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

  // --ts-lower-bound 1967446102500
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

  // Init the trace environment --> IMPORTANT
  TraceEnvironment::initialize();
  // Init runtime and set threads to use --> IMPORTANT
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.thread_terminated_callback = [](std::string_view thread_name) {
    std::cout << "thread " << thread_name << "finished" << std::endl;
  };
  concurren_options.max_background_threads = 4;
  concurren_options.max_cpu_threads = 10;
  concurren_options.max_background_executor_waiting_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(600));
  concurren_options.max_thread_pool_executor_waiting_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(600));
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();
  const auto background_executor = runtime.background_executor();
  const auto thread_executor = runtime.thread_executor();
  throw_if_empty(thread_pool_executor, resume_executor_null);
  throw_if_empty(background_executor, resume_executor_null);
  throw_if_empty(thread_executor, resume_executor_null);

  //const std::string jaeger_url = "http://localhost:4318/v1/traces";
  std::string jaeger_url = "http://jaeger:4318/v1/traces";
  simbricks::trace::OtlpSpanExporter
      exporter{jaeger_url, false, "trace"};
  //simbricks::trace::NoOpExporter exporter;

  Tracer tracer{exporter};

  constexpr size_t kAmountSources = 4;
  constexpr size_t kLineBufferSize = 1;
  constexpr size_t kEventBufferSize = 35'000'000;
  //Timer timer{kAmountSources};
  WeakTimer timer{kAmountSources};

  const std::set<std::string> blacklist_functions{
      "match_strlcpy", "__const_udelay", "static_key_disable", "__put_page", "relock_page_lruvec_irqsave",
      "xas_move_index", "xas_set_offset", "blk_account_io_merge_bio", "biovec_phys_mergeable.isra.0"
  };

  using QueueT = UnBoundedChannel<std::shared_ptr<Context>>;
  auto server_hn = create_shared<QueueT>(channel_is_null);
  auto server_nh = create_shared<QueueT>(channel_is_null);
  auto client_hn = create_shared<QueueT>(channel_is_null);
  auto client_nh = create_shared<QueueT>(channel_is_null);
  auto nic_cn = create_shared<QueueT>(channel_is_null);
  auto nic_sn = create_shared<QueueT>(channel_is_null);
  auto server_n_h_receive = create_shared<QueueT>(channel_is_null);
  auto client_n_h_receive = create_shared<QueueT>(channel_is_null);

  std::vector<EventTimestampFilter::EventTimeBoundary> timestamp_bounds{
      EventTimestampFilter::EventTimeBoundary{lower_bound, upper_bound}};

  auto spanner_h_s = create_shared<HostSpanner>(spanner_is_null,
                                                "Server-Host",
                                                tracer,
                                                timer,
                                                server_hn,
                                                server_nh,
                                                server_n_h_receive,
                                                false);

  auto spanner_h_c = create_shared<HostSpanner>(spanner_is_null,
                                                "Client-Host",
                                                tracer,
                                                timer,
                                                client_hn,
                                                client_nh,
                                                client_n_h_receive,
                                                true);

  auto spanner_n_s = create_shared<NicSpanner>(spanner_is_null,
                                               "NIC-Server",
                                               tracer,
                                               timer,
                                               nic_sn,
                                               nic_cn,
                                               server_nh,
                                               server_hn,
                                               server_n_h_receive);

  auto spanner_n_c = create_shared<NicSpanner>(spanner_is_null,
                                               "Client-NIC",
                                               tracer,
                                               timer,
                                               nic_cn,
                                               nic_sn,
                                               client_nh,
                                               client_hn,
                                               client_n_h_receive);

  if (result.count("gem5-server-event-stream") and result.count("gem5-client-event-stream")
      and result.count("nicbm-server-event-stream") and result.count("nicbm-client-event-stream")) {

    auto parser_h_s = create_shared<EventStreamParser>("EventStreamParser null",
                                                       "gem5-server-reader",
                                                       result["gem5-server-event-stream"].as<std::string>());
    auto filter_h_s = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_h_s{filter_h_s};
    const pipeline<std::shared_ptr<Event>> pl_h_s{parser_h_s, pipeline_h_s, spanner_h_s};

    auto parser_h_c = create_shared<EventStreamParser>("EventStreamParser",
                                                       "gem5-client-reader",
                                                       result["gem5-client-event-stream"].as<std::string>());
    auto filter_h_c = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_h_c{filter_h_c};
    const pipeline<std::shared_ptr<Event>> pl_h_c{parser_h_c, pipeline_h_c, spanner_h_c};

    auto parser_n_s = create_shared<EventStreamParser>("EventStreamParser null",
                                                       "nicbm-server-reader",
                                                       result["nicbm-server-event-stream"].as<std::string>());
    auto filter_n_s = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_n_s{filter_n_s};
    const pipeline<std::shared_ptr<Event>> pl_n_s{parser_n_s, pipeline_n_s, spanner_n_s};

    auto parser_n_c = create_shared<EventStreamParser>("EventStreamParser null",
                                                       "nicbm-client-reader",
                                                       result["nicbm-client-event-stream"].as<std::string>());
    auto filter_n_c = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_n_c{filter_h_c};
    const pipeline<std::shared_ptr<Event>> pl_n_c{parser_n_c, pipeline_n_c, spanner_n_c};

    std::vector<pipeline<std::shared_ptr<Event>>> pipelines{pl_h_c, pl_n_c, pl_h_s, pl_n_s};
    run_pipelines_parallel(thread_pool_executor, pipelines);

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
      !TraceEnvironment::add_symbol_table(
          "Linuxvm-Symbols",
          background_executor, thread_pool_executor,
          result["linux-dump-server-client"].as<std::string>(), 0,
          FilterType::kS)) {
    std::cerr << "could not initialize symbol table linux-dump-server-client"
              << std::endl;
    exit(EXIT_FAILURE);
  }
  if (result.count("nic-i40e-dump") &&
      !TraceEnvironment::add_symbol_table(
          "Nicdriver-Symbols",
          background_executor, thread_pool_executor,
          result["nic-i40e-dump"].as<std::string>(),
          0xffffffffa0000000ULL, FilterType::kS)) {
    std::cerr << "could not initialize symbol table nic-i40e-dump" << std::endl;
    exit(EXIT_FAILURE);
  }

  // SERVER HOST PIPELINE
  std::set<EventType> to_filter{EventType::kHostInstrT, EventType::kSimProcInEventT, EventType::kSimSendSyncT};
  auto event_filter_h_s = create_shared<EventTypeFilter>(actor_is_null, to_filter, true);
  auto timestamp_filter_h_s = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
  ComponentFilter comp_filter_server("ComponentFilter-Server");
  Gem5Parser gem5_server_par{thread_pool_executor,
                             "Gem5ServerParser",
                             comp_filter_server};
  auto gem5_ser_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
      "BufferedEventProvider null",
      thread_pool_executor,
      background_executor,
      thread_executor,
      "Gem5ServerEventProvider",
      result["gem5-log-server"].as<std::string>(),
      gem5_server_par
  );
  std::ofstream out_h_s;
  auto printer_h_s = createPrinter(out_h_s, result, "gem5-server-events", true);
  if (not printer_h_s) {
    exit(EXIT_FAILURE);
  }
  auto func_filter_h_s = create_shared<HostCallFuncFilter>(actor_is_null, blacklist_functions, true);
  std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
      server_host_pipes{timestamp_filter_h_s, event_filter_h_s, func_filter_h_s, printer_h_s};
  const pipeline<std::shared_ptr<Event>> server_host_pipeline{
      gem5_ser_buf_pro, server_host_pipes, spanner_h_s};
  //std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
  //    server_host_pipes{timestamp_filter_h_s, event_filter_h_s};
  //const pipeline<std::shared_ptr<Event>> server_host_pipeline{
  //    gem5_server_par, server_host_pipes, printer_h_s};

  // CLIENT HOST PIPELINE
  auto event_filter_h_c = create_shared<EventTypeFilter>(actor_is_null, to_filter, true);
  auto timestamp_filter_h_c = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
  ComponentFilter comp_filter_client("ComponentFilter-Server");
  Gem5Parser gem5_client_par{thread_pool_executor,
                             "Gem5ClientParser",
                             comp_filter_client};
  auto gem5_client_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
      "BufferedEventProvider null",
      thread_pool_executor,
      background_executor,
      thread_executor,
      "Gem5ClientEventProvider",
      result["gem5-log-client"].as<std::string>(),
      gem5_client_par
  );
  std::ofstream out_h_c;
  auto printer_h_c = createPrinter(out_h_c, result, "gem5-client-events", true);
  if (not printer_h_c) {
    exit(EXIT_FAILURE);
  }
  auto func_filter_h_c = create_shared<HostCallFuncFilter>(actor_is_null, blacklist_functions, true);
  std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
      client_host_pipes{timestamp_filter_h_c, event_filter_h_c, func_filter_h_c, printer_h_c};
  const pipeline<std::shared_ptr<Event>> client_host_pipeline{
      gem5_client_buf_pro, client_host_pipes, spanner_h_c};
  //std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
  //    client_host_pipes{timestamp_filter_h_c, event_filter_h_c};
  //const pipeline<std::shared_ptr<Event>> client_host_pipeline{
  //    gem5_client_par, client_host_pipes, printer_h_c};

  // SERVER NIC PIPELINE
  auto event_filter_n_s = create_shared<EventTypeFilter>(actor_is_null, to_filter, true);
  auto timestamp_filter_n_s = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
  NicBmParser nicbm_ser_par{thread_pool_executor,
                            "NicbmServerParser"};
  auto nicbm_ser_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
      "BufferedEventProvider null",
      thread_pool_executor,
      background_executor,
      thread_executor,
      "NicbmServerEventProvider",
      result["nicbm-log-server"].as<std::string>(),
      nicbm_ser_par
  );
  std::ofstream out_n_s;
  auto printer_n_s = createPrinter(out_n_s, result, "nicbm-server-events", true);
  if (not printer_n_s) {
    exit(EXIT_FAILURE);
  }
  std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
      server_nic_pipes{timestamp_filter_n_s, event_filter_n_s, printer_n_s};
  const pipeline<std::shared_ptr<Event>> server_nic_pipeline{
      nicbm_ser_buf_pro, server_nic_pipes, spanner_n_s};
  //std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
  //    server_nic_pipes{timestamp_filter_n_s, event_filter_n_s};
  //const pipeline<std::shared_ptr<Event>> server_nic_pipeline{
  //    nic_ser_par, server_nic_pipes, printer_n_s};

  // CLIENT NIC PIPELINE
  auto event_filter_n_c = create_shared<EventTypeFilter>(actor_is_null, to_filter, true);
  auto timestamp_filter_n_c = create_shared<EventTimestampFilter>(actor_is_null, timestamp_bounds);
  NicBmParser nicbm_client_par{thread_pool_executor,
                               "NicbmClientParser"};
  auto nicbm_client_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
      "BufferedEventProvider null",
      thread_pool_executor,
      background_executor,
      thread_executor,
      "NicbmClientEventProvider",
      result["nicbm-log-client"].as<std::string>(),
      nicbm_client_par
  );
  std::ofstream out_n_c;
  auto printer_n_c = createPrinter(out_n_c, result, "nicbm-client-events", true);
  if (not printer_n_c) {
    exit(EXIT_FAILURE);
  }
  std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
      client_nic_pipes{timestamp_filter_n_c, event_filter_n_c, printer_n_c};
  const pipeline<std::shared_ptr<Event>> client_nic_pipeline{
      nicbm_client_buf_pro, client_nic_pipes, spanner_n_c};
  //std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
  //    client_nic_pipes{timestamp_filter_n_c, event_filter_n_c};
  //const pipeline<std::shared_ptr<Event>> client_nic_pipeline{
  //    nic_cli_par, client_nic_pipes, printer_n_c};

  std::vector<pipeline<std::shared_ptr<Event>>>
      pipelines{client_host_pipeline, client_nic_pipeline, server_host_pipeline, server_nic_pipeline};
  run_pipelines_parallel(thread_pool_executor, pipelines);

  std::cout << "runtime goes out of scope!!!!!" << std::endl;

  exit(EXIT_SUCCESS);
}
