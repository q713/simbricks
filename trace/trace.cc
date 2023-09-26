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

#include "sync/corobelt.h"
#include "config/config.h"
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

std::shared_ptr<EventPrinter> createPrinter(std::ofstream &out,
                                            cxxopts::ParseResult &result,
                                            const std::string &option,
                                            bool allow_override) {
  std::shared_ptr<EventPrinter> printer;
  if (result.count(option) != 0) {
    try {
      CreateOpenFile(out, result[option].as<std::string>(), allow_override);
      printer = create_shared<EventPrinter>(TraceException::kPrinterIsNull, out);
    } catch (TraceException &exe) {
      std::cerr << "could not create printer: " << exe.what() << '\n';
      return nullptr;
    }
  } else {
    printer = create_shared<EventPrinter>(TraceException::kPrinterIsNull, std::cout);
  }

  return printer;
}

int main(int argc, char *argv[]) {

  cxxopts::Options options("trace", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")
      ("trace-env-config",
       "file path to a trace environment config yaml file",
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
    std::cerr << "Could not parse cli options: " << e.what() << '\n';
    exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << '\n';
    exit(EXIT_SUCCESS);
  }

  // --ts-lower-bound 1967446102500
  // TODO: move into configuration file! --> implement better start @time mechanism !!!
  uint64_t lower_bound = EventTimeBoundary::kMinLowerBound;
  uint64_t upper_bound = EventTimeBoundary::kMaxUpperBound;

  if (result.count("ts-upper-bound")) {
    sim_string_utils::parse_uint_trim_copy(
        result["ts-upper-bound"].as<std::string>(), 10, &upper_bound);
  }

  if (result.count("ts-lower-bound")) {
    sim_string_utils::parse_uint_trim_copy(
        result["ts-lower-bound"].as<std::string>(), 10, &lower_bound);
  }

  // Init the trace environment --> IMPORTANT
  if (1 > result.count("trace-env-config")) {
    std::cerr << "must provide a path to a yaml trace environment configuration file" << '\n';
    exit(EXIT_FAILURE);
  }
  const TraceEnvConfig trace_env_config
      = TraceEnvConfig::CreateFromYaml(result["trace-env-config"].as<std::string>());
  TraceEnvironment trace_environment{trace_env_config};

  simbricks::trace::OtlpSpanExporter exporter{trace_environment,
                                              trace_env_config.GetJaegerUrl(),
                                              false,
                                              "trace"};
  //simbricks::trace::NoOpExporter exporter{trace_environment};

  Tracer tracer{trace_environment, exporter};

  constexpr size_t kAmountSources = 4;
  constexpr size_t kLineBufferSize = 1;
  constexpr size_t kEventBufferSize = 100'000'000;
  //Timer timer{kAmountSources};
  WeakTimer timer{kAmountSources};

  const std::set<std::string> blacklist_functions{trace_env_config.BeginBlacklistFuncIndicator(),
                                                  trace_env_config.EndBlacklistFuncIndicator()};

  try {
    using QueueT = CoroUnBoundedChannel<std::shared_ptr<Context>>;
    auto server_hn = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto server_nh = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto client_hn = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto client_nh = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto nic_cn = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto nic_sn = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto server_n_h_receive = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto client_n_h_receive = create_shared<QueueT>(TraceException::kChannelIsNull);

    std::vector<EventTimeBoundary> timestamp_bounds{EventTimeBoundary{lower_bound, upper_bound}};

    auto spanner_h_s = create_shared<HostSpanner>(TraceException::kSpannerIsNull,
                                                  trace_environment,
                                                  "Server-Host",
                                                  tracer,
                                                  server_hn,
                                                  server_nh,
                                                  server_n_h_receive);

    auto spanner_h_c = create_shared<HostSpanner>(TraceException::kSpannerIsNull,
                                                  trace_environment,
                                                  "Client-Host",
                                                  tracer,
                                                  client_hn,
                                                  client_nh,
                                                  client_n_h_receive);

    auto spanner_n_s = create_shared<NicSpanner>(TraceException::kSpannerIsNull,
                                                 trace_environment,
                                                 "NIC-Server",
                                                 tracer,
                                                 nic_sn,
                                                 nic_cn,
                                                 server_nh,
                                                 server_hn,
                                                 server_n_h_receive);

    auto spanner_n_c = create_shared<NicSpanner>(TraceException::kSpannerIsNull,
                                                 trace_environment,
                                                 "Client-NIC",
                                                 tracer,
                                                 nic_cn,
                                                 nic_sn,
                                                 client_nh,
                                                 client_hn,
                                                 client_n_h_receive);

    if (result.count("gem5-server-event-stream") and result.count("gem5-client-event-stream")
        and result.count("nicbm-server-event-stream") and result.count("nicbm-client-event-stream")) {

      auto parser_h_s = create_shared<EventStreamParser>("EventStreamParser null",
                                                         trace_environment,
                                                         "gem5-server-reader",
                                                         result["gem5-server-event-stream"].as<std::string>());
      auto filter_h_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);
      std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_h_s{filter_h_s};
      const pipeline<std::shared_ptr<Event>> pl_h_s{parser_h_s, pipeline_h_s, spanner_h_s};

      auto parser_h_c = create_shared<EventStreamParser>("EventStreamParser",
                                                         trace_environment,
                                                         "gem5-client-reader",
                                                         result["gem5-client-event-stream"].as<std::string>());
      auto filter_h_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);
      std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_h_c{filter_h_c};
      const pipeline<std::shared_ptr<Event>> pl_h_c{parser_h_c, pipeline_h_c, spanner_h_c};

      auto parser_n_s = create_shared<EventStreamParser>("EventStreamParser null",
                                                         trace_environment,
                                                         "nicbm-server-reader",
                                                         result["nicbm-server-event-stream"].as<std::string>());
      auto filter_n_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);
      std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_n_s{filter_n_s};
      const pipeline<std::shared_ptr<Event>> pl_n_s{parser_n_s, pipeline_n_s, spanner_n_s};

      auto parser_n_c = create_shared<EventStreamParser>("EventStreamParser null",
                                                         trace_environment,
                                                         "nicbm-client-reader",
                                                         result["nicbm-client-event-stream"].as<std::string>());
      auto filter_n_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);
      std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>> pipeline_n_c{filter_h_c};
      const pipeline<std::shared_ptr<Event>> pl_n_c{parser_n_c, pipeline_n_c, spanner_n_c};

      std::vector<pipeline<std::shared_ptr<Event>>> pipelines{pl_h_c, pl_n_c, pl_h_s, pl_n_s};
      run_pipelines_parallel(trace_environment.GetPoolExecutor(), pipelines);

      exit(EXIT_SUCCESS);
    }

    if (!result.count("gem5-log-server") ||
        !result.count("nicbm-log-server") ||
        !result.count("gem5-log-client") ||
        !result.count("nicbm-log-client")) {
      std::cerr << "invalid arguments given" << '\n'
                << options.help() << '\n';
      exit(EXIT_FAILURE);
    }

    // SERVER HOST PIPELINE
    const std::set<EventType> to_filter{trace_env_config.BeginTypesToFilter(), trace_env_config.EndTypesToFilter()};

    auto event_filter_h_s = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
                                                           //false);
    auto timestamp_filter_h_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    const ComponentFilter comp_filter_server("ComponentFilter-Server");
    Gem5Parser gem5_server_par{trace_environment,
                               "Gem5ServerParser",
                               comp_filter_server};
    auto gem5_ser_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
        "BufferedEventProvider null",
        trace_environment,
        "Gem5ServerEventProvider",
        result["gem5-log-server"].as<std::string>(),
        gem5_server_par,
        timer
    );
    std::ofstream out_h_s;
    auto printer_h_s = createPrinter(out_h_s, result, "gem5-server-events", true);
    if (not printer_h_s) {
      exit(EXIT_FAILURE);
    }
    auto func_filter_h_s = create_shared<HostCallFuncFilter>(TraceException::kActorIsNull,
                                                             trace_environment,
                                                             blacklist_functions,
                                                             true);
    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
        server_host_pipes{timestamp_filter_h_s, event_filter_h_s, func_filter_h_s, printer_h_s};
    const pipeline<std::shared_ptr<Event>> server_host_pipeline{
        gem5_ser_buf_pro, server_host_pipes, spanner_h_s};
    //std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
    //    server_host_pipes{timestamp_filter_h_s, event_filter_h_s};
    //const pipeline<std::shared_ptr<Event>> server_host_pipeline{
    //    gem5_ser_buf_pro, server_host_pipes, printer_h_s};

    // CLIENT HOST PIPELINE
    auto event_filter_h_c = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
                                                           //false);
    auto timestamp_filter_h_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    const ComponentFilter comp_filter_client("ComponentFilter-Server");
    Gem5Parser gem5_client_par{trace_environment,
                               "Gem5ClientParser",
                               comp_filter_client};
    auto gem5_client_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
        "BufferedEventProvider null",
        trace_environment,
        "Gem5ClientEventProvider",
        result["gem5-log-client"].as<std::string>(),
        gem5_client_par,
        timer
    );
    std::ofstream out_h_c;
    auto printer_h_c = createPrinter(out_h_c, result, "gem5-client-events", true);
    if (not printer_h_c) {
      exit(EXIT_FAILURE);
    }
    auto func_filter_h_c = create_shared<HostCallFuncFilter>(TraceException::kActorIsNull,
                                                             trace_environment,
                                                             blacklist_functions,
                                                             true);
    std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
        client_host_pipes{timestamp_filter_h_c, event_filter_h_c, func_filter_h_c, printer_h_c};
    const pipeline<std::shared_ptr<Event>> client_host_pipeline{
        gem5_client_buf_pro, client_host_pipes, spanner_h_c};
    //std::vector<std::shared_ptr<cpipe<std::shared_ptr<Event>>>>
    //    client_host_pipes{timestamp_filter_h_c, event_filter_h_c};
    //const pipeline<std::shared_ptr<Event>> client_host_pipeline{
    //    gem5_client_buf_pro, client_host_pipes, printer_h_c};

    // SERVER NIC PIPELINE
    auto event_filter_n_s = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
                                                           //false);
    auto timestamp_filter_n_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    NicBmParser nicbm_ser_par{trace_environment,
                              "NicbmServerParser"};
    auto nicbm_ser_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
        "BufferedEventProvider null",
        trace_environment,
        "NicbmServerEventProvider",
        result["nicbm-log-server"].as<std::string>(),
        nicbm_ser_par,
        timer
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
    //    nicbm_ser_buf_pro, server_nic_pipes, printer_n_s};

    // CLIENT NIC PIPELINE
    auto event_filter_n_c = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
                                                           //false);
    auto timestamp_filter_n_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    NicBmParser nicbm_client_par{trace_environment,
                                 "NicbmClientParser"};
    auto nicbm_client_buf_pro = create_shared<BufferedEventProvider<kLineBufferSize, kEventBufferSize>>(
        "BufferedEventProvider null",
        trace_environment,
        "NicbmClientEventProvider",
        result["nicbm-log-client"].as<std::string>(),
        nicbm_client_par,
        timer
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
    //    nicbm_client_buf_pro, client_nic_pipes, printer_n_c};

    std::vector<pipeline<std::shared_ptr<Event>>>
        pipelines{client_host_pipeline, client_nic_pipeline, server_host_pipeline, server_nic_pipeline};
    run_pipelines_parallel(trace_environment.GetPoolExecutor(), pipelines);
  } catch (TraceException &err) {
    std::cerr << err.what() << '\n';
    exit(EXIT_FAILURE);
  }
  // std::cout << "runtime goes out of scope!!!!!" << std::endl;
  exit(EXIT_SUCCESS);
}
