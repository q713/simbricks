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

#include <catch2/catch_all.hpp>
#include <vector>
#include <memory>

#include "test-util.h"
#include "util/componenttable.h"
#include "reader/reader.h"
#include "parser/parser.h"
#include "util/factory.h"
#include "events/events.h"
#include "util/exception.h"

TEST_CASE("Test ns3 parser produces expected event stream", "[NS3Parser]") {
  const std::string test_file_path{"tests/raw-logs/ns3-raw-log.txt"};
  const std::string parser_name{"NS3Parser-test-parser"};

  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};

  ReaderBuffer<10> reader_buffer{"test-reader", true};
  REQUIRE_NOTHROW(reader_buffer.OpenFile(test_file_path));

  auto ns3_parser = create_shared<NS3Parser>(TraceException::kParserIsNull,
                                             trace_environment,
                                             parser_name);
  const uint64_t ident = ns3_parser->GetIdent();

  const NetworkEvent::NetworkDeviceType cosim_net_dev = NetworkEvent::NetworkDeviceType::kCosimNetDevice;
  const NetworkEvent::NetworkDeviceType simple_net_dev = NetworkEvent::NetworkDeviceType::kSimpleNetDevice;

  const auto within = NetworkEvent::EventBoundaryType::kWithinSimulator;
  const auto from = NetworkEvent::EventBoundaryType::kFromAdapter;
  const auto to = NetworkEvent::EventBoundaryType::kToAdapter;

  std::shared_ptr<Event> parsed_event;
  const std::vector<std::shared_ptr<Event>> to_match{
      std::make_shared<NetworkEnqueue>(1945871772000, ident, parser_name, 1, 2, cosim_net_dev,  42, from, CreateEthHeader(0x806, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkDequeue>(1945871772000, ident, parser_name, 1, 2, cosim_net_dev,  42, within, CreateEthHeader(0x806, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkEnqueue>(1945871772000, ident, parser_name, 1, 1, simple_net_dev, 42, within, CreateEthHeader(0x614f, 0x00, 0x01, 0xcc, 0x18, 0x61, 0xcf, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1945871772000, ident, parser_name, 1, 1, simple_net_dev, 42, within, CreateEthHeader(0x614f, 0x00, 0x01, 0xcc, 0x18, 0x61, 0xcf, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(1945871772000, ident, parser_name, 0, 1, simple_net_dev, 42, within, CreateEthHeader(0x614f, 0x00, 0x01, 0xcc, 0x18, 0x61, 0xcf, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1945871772000, ident, parser_name, 0, 1, simple_net_dev, 42, within, CreateEthHeader(0x614f, 0x00, 0x01, 0xcc, 0x18, 0x61, 0xcf, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(1945871772000, ident, parser_name, 0, 2, cosim_net_dev,  42, within, CreateEthHeader(0x614f, 0x00, 0x01, 0xcc, 0x18, 0x61, 0xcf, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1945871772000, ident, parser_name, 0, 2, cosim_net_dev,  42, to, CreateEthHeader(0x806, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkEnqueue>(1946404561000, ident, parser_name, 0, 2, cosim_net_dev,  42, from, CreateEthHeader(0x806, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f), std::nullopt),
      std::make_shared<NetworkDequeue>(1946404561000, ident, parser_name, 0, 2, cosim_net_dev,  42, within, CreateEthHeader(0x806, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f), std::nullopt),
      std::make_shared<NetworkEnqueue>(1946404561000, ident, parser_name, 0, 1, simple_net_dev, 42,  within, CreateEthHeader(0x6fb2, 0x00, 0x02, 0x5c, 0x1a, 0xf9, 0x8b, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1946404561000, ident, parser_name, 0, 1, simple_net_dev, 42,  within, CreateEthHeader(0x6fb2, 0x00, 0x02, 0x5c, 0x1a, 0xf9, 0x8b, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(1946404561000, ident, parser_name, 1, 1, simple_net_dev, 42,  within, CreateEthHeader(0x6fb2, 0x00, 0x02, 0x5c, 0x1a, 0xf9, 0x8b, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1946404561000, ident, parser_name, 1, 1, simple_net_dev, 42,  within, CreateEthHeader(0x6fb2, 0x00, 0x02, 0x5c, 0x1a, 0xf9, 0x8b, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(1946404561000, ident, parser_name, 1, 2, cosim_net_dev,  42, within, CreateEthHeader(0x6fb2, 0x00, 0x02, 0x5c, 0x1a, 0xf9, 0x8b, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1946404561000, ident, parser_name, 1, 2, cosim_net_dev,  42, to, CreateEthHeader(0x806, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f), std::nullopt),
      std::make_shared<NetworkEnqueue>(1946922071000, ident, parser_name, 1, 2, cosim_net_dev,  98, from, CreateEthHeader(0x800, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2), std::nullopt),
      std::make_shared<NetworkDequeue>(1946922071000, ident, parser_name, 1, 2, cosim_net_dev,  98, within, CreateEthHeader(0x800, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2), std::nullopt),
      std::make_shared<NetworkEnqueue>(1946922071000, ident, parser_name, 1, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xcc, 0x6d, 0x45, 0x00, 0x00, 0x54, 0x6c, 0xe7), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64 ,1)),
      std::make_shared<NetworkDequeue>(1946922071000, ident, parser_name, 1, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xcc, 0x6d, 0x45, 0x00, 0x00, 0x54, 0x6c, 0xe7), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64 ,1)),
      std::make_shared<NetworkEnqueue>(1946922071000, ident, parser_name, 0, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xcc, 0x6d, 0x45, 0x00, 0x00, 0x54, 0x6c, 0xe7), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64 ,1)),
      std::make_shared<NetworkDequeue>(1946922071000, ident, parser_name, 0, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xcc, 0x6d, 0x45, 0x00, 0x00, 0x54, 0x6c, 0xe7), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64 ,1)),
      std::make_shared<NetworkEnqueue>(1946922071000, ident, parser_name, 0, 2, cosim_net_dev,  98, within, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xcc, 0x6d, 0x45, 0x00, 0x00, 0x54, 0x6c, 0xe7), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64 ,1)),
      std::make_shared<NetworkDequeue>(1946922071000, ident, parser_name, 0, 2, cosim_net_dev,  98, to, CreateEthHeader(0x800, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64 ,1)),
      std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 0, 2, cosim_net_dev,  98, from, CreateEthHeader(0x800, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f), std::nullopt),
      std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 0, 2, cosim_net_dev,  98, within, CreateEthHeader(0x800, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f), std::nullopt),
      std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 0, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 0, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98,  within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 2, cosim_net_dev,  98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 2, cosim_net_dev,  98, to, CreateEthHeader(0x800, 0x5c, 0x1a, 0xf9, 0x8b, 0x6f, 0xb2, 0xcc, 0x18, 0x61, 0xcf, 0x61, 0x4f), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2))
  };
  std::pair<bool, LineHandler *> bh_p;
  for (const auto &match : to_match) {
    REQUIRE(reader_buffer.HasStillLine());
    REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
    REQUIRE(bh_p.first);
    LineHandler &line_handler = *bh_p.second;
    parsed_event = ns3_parser->ParseEvent(line_handler).run().get();
    std::cout << parsed_event << std::endl;
    REQUIRE(parsed_event);
    REQUIRE(parsed_event->Equal(*match));
  }

  REQUIRE_FALSE(reader_buffer.HasStillLine());
  REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
  REQUIRE_FALSE(bh_p.first);
  REQUIRE(bh_p.second == nullptr);
}