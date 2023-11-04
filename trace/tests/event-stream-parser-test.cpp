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

#include "util/componenttable.h"
#include "reader/reader.h"
#include "parser/parser.h"
#include "parser/eventStreamParser.h"
#include "util/factory.h"
#include "events/events.h"

constexpr std::optional<NetworkEvent::EthernetHeader> CreateEthHeader(size_t length_type,
                                                                      unsigned char src_m1,
                                                                      unsigned char src_m2,
                                                                      unsigned char src_m3,
                                                                      unsigned char src_m4,
                                                                      unsigned char src_m5,
                                                                      unsigned char src_m6,
                                                                      unsigned char dst_m1,
                                                                      unsigned char dst_m2,
                                                                      unsigned char dst_m3,
                                                                      unsigned char dst_m4,
                                                                      unsigned char dst_m5,
                                                                      unsigned char dst_m6) {
  NetworkEvent::EthernetHeader header;
  header.length_type_ = length_type;
  header.src_mac_ = std::array<unsigned char, 6>{src_m1, src_m2, src_m3, src_m4, src_m5, src_m6};
  header.dst_mac_ = std::array<unsigned char, 6>{dst_m1, dst_m2, dst_m3, dst_m4, dst_m5, dst_m6};
  return header;
}

constexpr std::optional<NetworkEvent::Ipv4Header> CreateIpHeader(size_t length,
                                                               uint32_t src_o1,
                                                               uint32_t src_o2,
                                                               uint32_t src_o3,
                                                               uint32_t src_o4,
                                                               uint32_t dst_o1,
                                                               uint32_t dst_o2,
                                                               uint32_t dst_o3,
                                                               uint32_t dst_o4) {
  NetworkEvent::Ipv4Header header;
  header.length_ = length;
  header.src_ip_ = (src_o1 << 24) | (src_o2 << 16) | (src_o3 << 8) | src_o4;
  header.dst_ip_ = (dst_o1 << 24) | (dst_o2 << 16) | (dst_o3 << 8) | dst_o4;
  return header;
}

TEST_CASE("Test event stream parser produces expected event stream", "[EventStreamParser]") {
  const std::string test_file_path{"tests/stream-parser-test-files/event-stream-parser-test.txt"};
  const std::string parser_name{"NS3Parser-test-parser"};

  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};

  ReaderBuffer<10> reader_buffer{"test-reader", true};
  REQUIRE_NOTHROW(reader_buffer.OpenFile(test_file_path));

  EventStreamParser event_stream_parser {trace_environment, parser_name};
  const uint64_t ident = 1;

  const std::string *cosim_net_dev = trace_environment.InternalizeAdditional("ns3::CosimNetDevice");
  const std::string *simple_net_dev = trace_environment.InternalizeAdditional("ns3::SimpleNetDevice");

  const std::vector<std::shared_ptr<Event>> to_match{
      std::make_shared<NetworkEnqueue>(1924400059000, ident, parser_name, 1, 2, cosim_net_dev, 42, CreateEthHeader(0x806, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkDequeue>(1924400059000, ident, parser_name, 1, 2, cosim_net_dev, 42, CreateEthHeader(0x806, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkEnqueue>(1924400059000, ident, parser_name, 1, 1, simple_net_dev, 42, CreateEthHeader(0x3bb4, 0x00, 0x01, 0x7c, 0x19, 0x62, 0x99, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1924400059000, ident, parser_name, 1, 1, simple_net_dev, 42, CreateEthHeader(0x3bb4, 0x00, 0x01, 0x7c, 0x19, 0x62, 0x99, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(1924400059000, ident, parser_name, 0, 2, cosim_net_dev, 42, CreateEthHeader(0x3bb4, 0x00, 0x01, 0x7c, 0x19, 0x62, 0x99, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(1924400059000, ident, parser_name, 0, 2, cosim_net_dev, 42, CreateEthHeader (0x806, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkEnqueue>(2984854389000, ident, parser_name, 1, 2, cosim_net_dev, 42, CreateEthHeader(0x806, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkDequeue>(2984854389000, ident, parser_name, 1, 2, cosim_net_dev, 42, CreateEthHeader(0x806, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkEnqueue>(2984854389000, ident, parser_name, 1, 1, simple_net_dev, 42, CreateEthHeader(0x3bb4, 0x00, 0x01, 0x7c, 0x19, 0x62, 0x99, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(2984854389000, ident, parser_name, 1, 1, simple_net_dev, 42, CreateEthHeader(0x3bb4, 0x00, 0x01, 0x7c, 0x19, 0x62, 0x99, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(2984854389000, ident, parser_name, 0, 2, cosim_net_dev, 42, CreateEthHeader(0x3bb4, 0x00, 0x01, 0x7c, 0x19, 0x62, 0x99, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(2984854389000, ident, parser_name, 0, 2, cosim_net_dev, 42, CreateEthHeader (0x806, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff), std::nullopt),
      std::make_shared<NetworkEnqueue>(2985386828000, ident, parser_name, 0, 2, cosim_net_dev, 42, CreateEthHeader(0x806, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4), std::nullopt),
      std::make_shared<NetworkDequeue>(2985386828000, ident, parser_name, 0, 2, cosim_net_dev, 42, CreateEthHeader(0x806, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4), std::nullopt),
      std::make_shared<NetworkEnqueue>(2985386828000, ident, parser_name, 0, 1, simple_net_dev, 42, CreateEthHeader(0x29db, 0x00, 0x02, 0x08, 0x7b, 0xaf, 0x8a, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(2985386828000, ident, parser_name, 0, 1, simple_net_dev, 42, CreateEthHeader(0x29db, 0x00, 0x02, 0x08, 0x7b, 0xaf, 0x8a, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkEnqueue>(2985386828000, ident, parser_name, 1, 2, cosim_net_dev, 42, CreateEthHeader(0x29db, 0x00, 0x02, 0x08, 0x7b, 0xaf, 0x8a, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04), std::nullopt),
      std::make_shared<NetworkDequeue>(2985386828000, ident, parser_name, 1, 2, cosim_net_dev, 42, CreateEthHeader (0x806, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4), std::nullopt),

      std::make_shared<NetworkEnqueue>(2985903382000, ident, parser_name, 1, 2, cosim_net_dev,  98, CreateEthHeader(0x800, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb), std::nullopt),
      std::make_shared<NetworkDequeue>(2985903382000, ident, parser_name, 1, 2, cosim_net_dev,  98, CreateEthHeader(0x800, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb), std::nullopt),
      std::make_shared<NetworkEnqueue>(2985903382000, ident, parser_name, 1, 1, simple_net_dev, 98, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xd3, 0x58, 0x45, 0x00, 0x00, 0x54, 0x65, 0xfc), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 1)),
      std::make_shared<NetworkDequeue>(2985903382000, ident, parser_name, 1, 1, simple_net_dev, 98, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xd3, 0x58, 0x45, 0x00, 0x00, 0x54, 0x65, 0xfc), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 1)),
      std::make_shared<NetworkEnqueue>(2985903382000, ident, parser_name, 0, 2, cosim_net_dev,  98, CreateEthHeader(0xc0a8, 0x40, 0x00, 0x40, 0x01, 0xd3, 0x58, 0x45, 0x00, 0x00, 0x54, 0x65, 0xfc), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 1)),
      std::make_shared<NetworkDequeue>(2985903382000, ident, parser_name, 0, 2, cosim_net_dev,  98, CreateEthHeader(0x800, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb), CreateIpHeader(84, 192, 168, 64, 2, 192, 168, 64, 1)),
      std::make_shared<NetworkEnqueue>(2986435512000, ident, parser_name, 0, 2, cosim_net_dev,  98, CreateEthHeader(0x800, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4), std::nullopt),
      std::make_shared<NetworkDequeue>(2986435512000, ident, parser_name, 0, 2, cosim_net_dev,  98, CreateEthHeader(0x800, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4), std::nullopt),
      std::make_shared<NetworkEnqueue>(2986435512000, ident, parser_name, 0, 1, simple_net_dev, 98, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x90, 0x06, 0x45, 0x00, 0x00, 0x54, 0xe9, 0x4e), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(2986435512000, ident, parser_name, 0, 1, simple_net_dev, 98, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x90, 0x06, 0x45, 0x00, 0x00, 0x54, 0xe9, 0x4e), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkEnqueue>(2986435512000, ident, parser_name, 1, 2, cosim_net_dev,  98, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x90, 0x06, 0x45, 0x00, 0x00, 0x54, 0xe9, 0x4e), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2)),
      std::make_shared<NetworkDequeue>(2986435512000, ident, parser_name, 1, 2, cosim_net_dev,  98, CreateEthHeader(0x800, 0x08, 0x7b, 0xaf, 0x8a, 0x29, 0xdb, 0x7c, 0x19, 0x62, 0x99, 0x3b, 0xb4), CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2))
  };
  std::shared_ptr<Event> parsed_event;
  std::pair<bool, LineHandler *> bh_p;
  for (const auto &match : to_match) {
    REQUIRE(reader_buffer.HasStillLine());
    REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
    REQUIRE(bh_p.first);
    LineHandler &line_handler = *bh_p.second;
    parsed_event = event_stream_parser.ParseEvent(line_handler).run().get();
    REQUIRE(parsed_event);
    REQUIRE(parsed_event->Equal(*match));
  }
  REQUIRE(reader_buffer.HasStillLine()); // stream is still good in principle
  REQUIRE_NOTHROW(bh_p = reader_buffer.NextHandler());
  REQUIRE_FALSE(bh_p.first);
  REQUIRE(bh_p.second == nullptr);
}