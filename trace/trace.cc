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

#include "spdlog/spdlog.h"

#include "sync/specializations.h"
#include "config/config.h"
#include "env/traceEnvironment.h"
#include "events/event-filter.h"
#include "parser/eventStreamParser.h"
#include "events/events.h"
#include "parser/parser.h"
#include "analytics/spanner.h"
#include "util/cxxopts.hpp"
#include "util/log.h"
#include "util/factory.h"
#include "exporter/exporter.h"
#include "events/printer.h"
#include "analytics/helper.h"

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

  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  // TODO : may be possible to "generate" this whole "script" using the yaml configuration, which in turn could be
  //        generated by using SimBricks orchestration framework
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
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
      ("ns3-log", "file path to a log file written by ns3", cxxopts::value<std::string>())
      ("ns3-events", "file to which the ns3 event stream is written to", cxxopts::value<std::string>())
      ("ts-lower-bound", "lower timestamp bound for events", cxxopts::value<std::string>())
      ("ts-upper-bound", "upper timestamp bound for events", cxxopts::value<std::string>())
      ("event-stream-log", "file path to file that stores an event stream", cxxopts::value<std::string>())
      ("gem5-server-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("gem5-client-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("nicbm-server-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("nicbm-client-event-stream", "create trace by using the event stream", cxxopts::value<std::string>())
      ("ns3-event-stream", "create trace by using the event stream", cxxopts::value<std::string>());

  cxxopts::ParseResult result;
  try {
    result = options.parse(argc, argv);

  } catch (cxxopts::exceptions::exception &e) {
    std::cerr << "Could not parse cli options: " << e.what() << '\n';
    throw_just(source_loc::current(), "cli parser error: ", e.what());
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
    throw_just(source_loc::current(), "no trace environment config given");
  }
  const TraceEnvConfig trace_env_config
      = TraceEnvConfig::CreateFromYaml(result["trace-env-config"].as<std::string>());
  TraceEnvironment trace_environment{trace_env_config};

  spdlog::set_level(trace_env_config.GetLogLevel());

//  auto exporter = create_shared<simbricks::trace::OtlpSpanExporter>(
//      TraceException::kSpanExporterNull,
//      trace_environment,
//      trace_env_config.GetJaegerUrl(),
//      false,
//      "trace");
  auto exporter = create_shared<simbricks::trace::NoOpExporter>(
      TraceException::kSpanExporterNull, trace_environment);

  Tracer tracer{trace_environment, std::move(exporter)};

  constexpr size_t kLineBufferSizePages = 256;
  constexpr bool kNamedPipes = true;
  const size_t event_buffer_size = trace_env_config.GetEventBufferSize();
  const std::set<std::string> blacklist_functions{trace_env_config.BeginBlacklistFuncIndicator(),
                                                  trace_env_config.EndBlacklistFuncIndicator()};

  try {
    using QueueT = CoroUnBoundedChannel<std::shared_ptr<Context>>;
    auto server_hn = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto server_nh = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto client_hn = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto client_nh = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto nic_c_to_network = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto nic_s_to_network = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto nic_s_from_network = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto nic_c_from_network = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto server_n_h_receive = create_shared<QueueT>(TraceException::kChannelIsNull);
    auto client_n_h_receive = create_shared<QueueT>(TraceException::kChannelIsNull);
    using SinkT = CoroChannelSink<std::shared_ptr<Context>>;
    auto sink_chan = create_shared<SinkT>(TraceException::kChannelIsNull);

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
                                                 nic_s_to_network,
                                                 nic_s_from_network,
                                                 server_nh,
                                                 server_hn,
                                                 server_n_h_receive);

    auto spanner_n_c = create_shared<NicSpanner>(TraceException::kSpannerIsNull,
                                                 trace_environment,
                                                 "Client-NIC",
                                                 tracer,
                                                 nic_c_to_network,
                                                 nic_c_from_network,
                                                 client_nh,
                                                 client_hn,
                                                 client_n_h_receive);

    NodeDeviceToChannelMap to_host_map;
    to_host_map.AddMapping(0, 2, nic_s_from_network);
    to_host_map.AddMapping(1, 2, nic_c_from_network);
    to_host_map.AddMapping(0, 3, sink_chan);
    to_host_map.AddMapping(1, 3, sink_chan);
    NodeDeviceToChannelMap from_host_map;
    from_host_map.AddMapping(0, 2, nic_s_to_network);
    from_host_map.AddMapping(1, 2, nic_c_to_network);
    from_host_map.AddMapping(0, 3, sink_chan);
    from_host_map.AddMapping(1, 3, sink_chan);
    // NOTE: this filtering could also be done using an event stream filter within the pipeline
    NodeDeviceFilter node_device_filter;
    node_device_filter.AddNodeDevice(0, 2);
    node_device_filter.AddNodeDevice(1, 2);
    node_device_filter.AddNodeDevice(0, 1);
    node_device_filter.AddNodeDevice(1, 1);

    auto spanner_ns3 = create_shared<NetworkSpanner>(TraceException::kSpannerIsNull,
                                                     trace_environment,
                                                     "NS3",
                                                     tracer,
                                                     from_host_map,
                                                     to_host_map,
                                                     node_device_filter);

    if (result.count("gem5-server-event-stream") and result.count("gem5-client-event-stream")
        and result.count("nicbm-server-event-stream") and result.count("nicbm-client-event-stream")
        and result.count("ns3-event-stream")) {

      auto parser_h_s = create_shared<EventStreamParser>("parser is null", trace_environment, "gem5-server-reader");
      auto event_pro_h_s = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
          TraceException::kBufferedEventProviderIsNull,
          trace_environment,
          "BufferedEventProviderHostServer",
          result["gem5-server-event-stream"].as<std::string>(),
          parser_h_s
      );
      auto filter_h_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);
      auto handler_h_s =
          create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
      handler_h_s->emplace_back(filter_h_s);
      auto pl_h_s = create_shared<Pipeline<std::shared_ptr<Event>>>(
          TraceException::kPipelineNull, event_pro_h_s, handler_h_s, spanner_h_s);

      auto parser_h_c = create_shared<EventStreamParser>("parser is null", trace_environment, "gem5-client-reader");
      auto event_pro_h_c = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
          TraceException::kBufferedEventProviderIsNull,
          trace_environment,
          "BufferedEventProviderHostClient",
          result["gem5-client-event-stream"].as<std::string>(),
          parser_h_c
      );
      auto filter_h_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);
      auto handler_h_c = create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
      handler_h_c->emplace_back(filter_h_c);
      auto pl_h_c = create_shared<Pipeline<std::shared_ptr<Event>>>(
          TraceException::kPipelineNull, event_pro_h_c, handler_h_c, spanner_h_c);

      auto parser_n_s = create_shared<EventStreamParser>("parser is null", trace_environment, "nicbm-server-reader");
      auto event_pro_n_s = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
          TraceException::kBufferedEventProviderIsNull,
          trace_environment,
          "BufferedEventProviderNicServer",
          result["nicbm-server-event-stream"].as<std::string>(),
          parser_n_s
      );
      auto filter_n_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);

      auto handler_n_s = create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
      handler_n_s->emplace_back(filter_n_s);
      auto pl_n_s = create_shared<Pipeline<std::shared_ptr<Event>>>(
          TraceException::kPipelineNull, event_pro_n_s, handler_n_s, spanner_n_s);

      auto parser_n_c = create_shared<EventStreamParser>("parser is null", trace_environment, "nicbm-client-reader");
      auto event_pro_n_c = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
          TraceException::kBufferedEventProviderIsNull,
          trace_environment,
          "BufferedEventProviderNicClient",
          result["nicbm-client-event-stream"].as<std::string>(),
          parser_n_c
      );
      auto filter_n_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);

      auto handler_n_c = create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
      handler_n_c->emplace_back(filter_n_c);
      auto pl_n_c = create_shared<Pipeline<std::shared_ptr<Event>>>(
          TraceException::kPipelineNull, event_pro_n_c, handler_n_c, spanner_n_c);

      auto parser_ns3 = create_shared<EventStreamParser>("parser is null", trace_environment, "ns3-event-parser");
      auto event_pro_ns3 = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
          TraceException::kBufferedEventProviderIsNull,
          trace_environment,
          "BufferedEventProviderNs3",
          result["ns3-event-stream"].as<std::string>(),
          parser_ns3
      );
      auto filter_ns3 = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                            trace_environment,
                                                            timestamp_bounds);

      auto handler_def_ns3 = create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>(
          "vector null");
      handler_def_ns3->emplace_back(filter_h_c);
      auto pipeline_def_ns3 = create_shared<Pipeline<std::shared_ptr<Event>>>(
          TraceException::kPipelineNull,
          event_pro_ns3,
          handler_def_ns3,
          spanner_ns3);

      auto pipelines = create_shared<std::vector<std::shared_ptr<Pipeline<std::shared_ptr<Event>>>>>("vector is null");
      pipelines->emplace_back(pl_h_c);
      pipelines->emplace_back(pl_n_c);
      pipelines->emplace_back(pl_h_s);
      pipelines->emplace_back(pl_n_s);
      pipelines->emplace_back(pipeline_def_ns3);
      spdlog::info("START TRACING PIPELINE FROM PREPROCESSED EVENT STREAM");
      RunPipelines<std::shared_ptr<Event>>(trace_environment.GetPoolExecutor(), pipelines);
      spdlog::info("FINISHED PIPELINE");
      exit(EXIT_SUCCESS);
    }

    if (!result.count("gem5-log-server") ||
        !result.count("nicbm-log-server") ||
        !result.count("gem5-log-client") ||
        !result.count("nicbm-log-client") ||
        !result.count("ns3-log")) {
      std::cerr << "invalid arguments given" << '\n'
                << options.help() << '\n';
      throw_just(source_loc::current(), "could not parse cmd arguments");
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
    auto gem5_server_par = create_shared<Gem5Parser>("parser is null", trace_environment,
                                                     "Gem5ServerParser",
                                                     comp_filter_server);
    auto gem5_ser_buf_pro = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
        TraceException::kBufferedEventProviderIsNull,
        trace_environment,
        "Gem5ServerEventProvider",
        result["gem5-log-server"].as<std::string>(),
        gem5_server_par
    );
    std::ofstream out_h_s;
    auto printer_h_s = createPrinter(out_h_s, result, "gem5-server-events", true);
    if (not printer_h_s) {
      throw_just(source_loc::current(), "could not create printer");
    }
    auto func_filter_h_s = create_shared<HostCallFuncFilter>(TraceException::kActorIsNull,
                                                             trace_environment,
                                                             blacklist_functions,
                                                             true);
    auto handler_server_host_pipeline =
        create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
    handler_server_host_pipeline->emplace_back(timestamp_filter_h_s);
    handler_server_host_pipeline->emplace_back(event_filter_h_s);
    handler_server_host_pipeline->emplace_back(func_filter_h_s);
    handler_server_host_pipeline->emplace_back(printer_h_s);
    auto server_host_pipeline = create_shared<Pipeline<std::shared_ptr<Event>>>(
        TraceException::kPipelineNull,
        gem5_ser_buf_pro,
        handler_server_host_pipeline,
        // printer_h_s);
        spanner_h_s);

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
    auto gem5_client_par = create_shared<Gem5Parser>("parser null", trace_environment,
                                                     "Gem5ClientParser",
                                                     comp_filter_client);
    auto gem5_client_buf_pro = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
        TraceException::kBufferedEventProviderIsNull,
        trace_environment,
        "Gem5ClientEventProvider",
        result["gem5-log-client"].as<std::string>(),
        gem5_client_par
    );
    std::ofstream out_h_c;
    auto printer_h_c = createPrinter(out_h_c, result, "gem5-client-events", true);
    if (not printer_h_c) {
      throw_just(source_loc::current(), "could not create printer");
      exit(EXIT_FAILURE);
    }
    auto func_filter_h_c = create_shared<HostCallFuncFilter>(TraceException::kActorIsNull,
                                                             trace_environment,
                                                             blacklist_functions,
                                                             true);
    auto handler_client_host_pipeline =
        create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
    handler_client_host_pipeline->emplace_back(timestamp_filter_h_c);
    handler_client_host_pipeline->emplace_back(event_filter_h_c);
    handler_client_host_pipeline->emplace_back(func_filter_h_c);
    handler_client_host_pipeline->emplace_back(printer_h_c);
    auto client_host_pipeline = create_shared<Pipeline<std::shared_ptr<Event>>>(
        TraceException::kPipelineNull,
        gem5_client_buf_pro,
        handler_client_host_pipeline,
        // printer_h_c);
        spanner_h_c);

    // SERVER NIC PIPELINE
    auto event_filter_n_s = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
    //false);
    auto timestamp_filter_n_s = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    auto nicbm_ser_par = create_shared<NicBmParser>("parser null", trace_environment,
                                                    "NicbmServerParser");
    auto nicbm_ser_buf_pro = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
        TraceException::kBufferedEventProviderIsNull,
        trace_environment,
        "NicbmServerEventProvider",
        result["nicbm-log-server"].as<std::string>(),
        nicbm_ser_par
    );
    std::ofstream out_n_s;
    auto printer_n_s = createPrinter(out_n_s, result, "nicbm-server-events", true);
    if (not printer_n_s) {
      throw_just(source_loc::current(), "could not create printer");
    }
    auto handler_server_nic_pipeline =
        create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
    handler_server_nic_pipeline->emplace_back(timestamp_filter_n_s);
    handler_server_nic_pipeline->emplace_back(event_filter_n_s);
    handler_server_nic_pipeline->emplace_back(printer_n_s);
    auto server_nic_pipeline = create_shared<Pipeline<std::shared_ptr<Event>>>(
        TraceException::kPipelineNull, nicbm_ser_buf_pro, handler_server_nic_pipeline,
        // printer_n_s);
        spanner_n_s);

    // CLIENT NIC PIPELINE
    auto event_filter_n_c = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
    //false);
    auto timestamp_filter_n_c = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    auto nicbm_client_par = create_shared<NicBmParser>("parser null", trace_environment,
                                                       "NicbmClientParser");
    auto nicbm_client_buf_pro = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
        TraceException::kBufferedEventProviderIsNull,
        trace_environment,
        "NicbmClientEventProvider",
        result["nicbm-log-client"].as<std::string>(),
        nicbm_client_par
    );
    std::ofstream out_n_c;
    auto printer_n_c = createPrinter(out_n_c, result, "nicbm-client-events", true);
    if (not printer_n_c) {
      throw_just(source_loc::current(), "could not create printer");
      exit(EXIT_FAILURE);
    }
    auto handler_client_nic_pipeline =
        create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
    handler_client_nic_pipeline->emplace_back(timestamp_filter_n_c);
    handler_client_nic_pipeline->emplace_back(event_filter_n_c);
    handler_client_nic_pipeline->emplace_back(printer_n_c);
    auto client_nic_pipeline = create_shared<Pipeline<std::shared_ptr<Event>>>(
        TraceException::kPipelineNull, nicbm_client_buf_pro, handler_client_nic_pipeline,
        // printer_n_c);
        spanner_n_c);

    // NS3 PIPELINE
    auto event_filter_ns3 = create_shared<EventTypeFilter>(TraceException::kActorIsNull,
                                                           trace_environment,
                                                           to_filter,
                                                           true);
    //false);
    auto timestamp_filter_ns3 = create_shared<EventTimestampFilter>(TraceException::kActorIsNull,
                                                                    trace_environment,
                                                                    timestamp_bounds);
    auto ns3_parser = create_shared<NS3Parser>("parser null", trace_environment, "NicbmClientParser");
    auto ns3_buf_pro = create_shared<BufferedEventProvider<kNamedPipes, kLineBufferSizePages>>(
        TraceException::kBufferedEventProviderIsNull,
        trace_environment,
        "Ns3EventProvider",
        result["ns3-log"].as<std::string>(),
        ns3_parser
    );
    std::ofstream out_ns3;
    auto printer_ns3 = createPrinter(out_ns3, result, "ns3-events", true);
    if (not printer_n_c) {
      throw_just(source_loc::current(), "could not create printer");
    }
    auto ns3_event_filter = create_shared<NS3EventFilter>(TraceException::kActorIsNull,
                                                          trace_environment,
                                                          node_device_filter);
    auto handler_ns3_pipeline =
        create_shared<std::vector<std::shared_ptr<Handler<std::shared_ptr<Event>>>>>("vector null");
    handler_ns3_pipeline->emplace_back(timestamp_filter_ns3);
    handler_ns3_pipeline->emplace_back(event_filter_ns3);
    handler_ns3_pipeline->emplace_back(ns3_event_filter);
    handler_ns3_pipeline->emplace_back(printer_ns3);
    auto ns3_pipeline = create_shared<Pipeline<std::shared_ptr<Event>>>(
        TraceException::kPipelineNull,
        ns3_buf_pro,
        handler_ns3_pipeline,
        // printer_ns3);
        spanner_ns3);

    auto pipelines = create_shared<std::vector<std::shared_ptr<Pipeline<std::shared_ptr<Event>>>>>("vector is null");
    pipelines->emplace_back(client_host_pipeline);
    pipelines->emplace_back(server_host_pipeline);
    pipelines->emplace_back(client_nic_pipeline);
    pipelines->emplace_back(server_nic_pipeline);
    pipelines->emplace_back(ns3_pipeline);
    spdlog::info("START TRACING PIPELINE FROM RAW SIMULATOR OUTPUT");
    RunPipelines<std::shared_ptr<Event>>(trace_environment.GetPoolExecutor(), pipelines);
    tracer.FinishExport();
    spdlog::info("FINISHED PIPELINE");

  } catch (TraceException &err) {
    std::cerr << err.what() << '\n';
    exit(EXIT_FAILURE);
  }
  exit(EXIT_SUCCESS);
}
