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
#include <memory>

#include "test-util.h"
#include "analytics/span.h"
#include "events/events.h"

TEST_CASE("Test HostMmioSpan", "[HostMmioSpan]") {

  const uint64_t source_id = 1;
  const size_t parser_ident = 1;
  const std::string parser_name = "test";
  std::string service_name = "test-service";

  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};
  auto trace_context = std::make_shared<TraceContext>(0, 0);

  /*
  HostMmioR: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374, id=94469376773312, addr=c0108000, size=4, bar=0, offset=0
  HostMmioCR: source_id=0, source_name=Gem5ClientParser, timestamp=1967469841374, id=94469376773312
  */
  SECTION("normal mmio read") {
    auto mmio_r = std::make_shared<HostMmioR>(1967468841374, parser_ident,
                                              parser_name, 94469376773312, 108000, 4, 0, 0);
    auto mmio_cr = std::make_shared<HostMmioCR>(1967469841374, parser_ident, parser_name, 94469376773312);

    HostMmioSpan span{trace_environment, trace_context, source_id, service_name, 0};

    REQUIRE(span.IsPending());
    REQUIRE(span.AddToSpan(mmio_r));
    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.AddToSpan(mmio_cr));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
  }

    /*
      HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374, id=94469376773312, addr=c0108000, size=4, bar=0, offset=0
      HostMmioCW: source_id=0, source_name=Gem5ClientParser, timestamp=1967469841374, id=94469376773312
      */
  SECTION("normal mmio write") {
    auto mmio_w = std::make_shared<HostMmioW>(1967468841374, parser_ident,
                                              parser_name, 94469376773312, 108000, 4, 0, 0, true);
    auto mmio_cw = std::make_shared<HostMmioCW>(1967469841374, parser_ident, parser_name, 94469376773312);

    HostMmioSpan span{trace_environment, trace_context, source_id, service_name, 0};

    REQUIRE(span.IsPending());
    REQUIRE(span.AddToSpan(mmio_w));
    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.AddToSpan(mmio_cw));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
  }

    /*
    HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374, id=94469376773312, addr=c0108000, size=4, bar=0, offset=0
    HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374
    HostMmioCW: source_id=0, source_name=Gem5ClientParser, timestamp=1967469841374, id=94469376773312
    */
  SECTION("posted mmio write") {
    auto mmio_w = std::make_shared<HostMmioW>(1967468841374, parser_ident,
                                              parser_name, 94469376773312, 108000, 4, 0, 0, true);
    auto mmio_imr = std::make_shared<HostMmioImRespPoW>(1967468841374, parser_ident, parser_name);

    HostMmioSpan span{trace_environment, trace_context, source_id, service_name, 0};

    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.AddToSpan(mmio_w));
    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.AddToSpan(mmio_imr));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
  }

    /*
    HostCall: source_id=0, source_name=Gem5ClientParser, timestamp=1967473336375, pc=ffffffff812c8a7c, func=pci_msix_write_vector_ctrl, comp=Linuxvm-Symbols <-----
    HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967473406749, id=94469376953344, addr=c040001c, size=4, bar=0, offset=0
    HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967473406749
    HostMmioR: source_id=0, source_name=Gem5ClientParser, timestamp=1967473531624, id=94469376953344, addr=c0400000, size=4, bar=0, offset=0
     */
  SECTION("mmio write cannot add additional read") {
    auto mmio_w = std::make_shared<HostMmioW>(1967473406749, parser_ident,
                                              parser_name, 94469376953344, 40001, 4, 0, 0, true);
    auto mmio_imr = std::make_shared<HostMmioImRespPoW>(1967473406749, parser_ident, parser_name);
    auto mmio_r = std::make_shared<HostMmioR>(1967473531624, parser_ident,
                                              parser_name, 94469376953344, 40000, 4, 0, 0);

    HostMmioSpan span{trace_environment, trace_context, source_id, service_name, 0};

    REQUIRE(span.IsPending());
    REQUIRE(span.AddToSpan(mmio_w));
    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.AddToSpan(mmio_imr));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
    REQUIRE_FALSE(span.AddToSpan(mmio_r));
    REQUIRE(span.IsComplete());
  }

  SECTION("mmio write non device BAR number") {
    auto mmio_w = std::make_shared<HostMmioW>(1967473406749, parser_ident,
                                              parser_name, 94469376953344, 40001, 4, 3, 0, true);
    auto mmio_imr = std::make_shared<HostMmioImRespPoW>(1967473406749, parser_ident, parser_name);

    HostMmioSpan span{trace_environment, trace_context, source_id, service_name, 3};

    REQUIRE(span.IsPending());
    REQUIRE(span.AddToSpan(mmio_w));
    REQUIRE(span.AddToSpan(mmio_imr));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
  }

  SECTION("mmio read no device BAR number") {
    auto mmio_r = std::make_shared<HostMmioR>(1967473531624, parser_ident,
                                              parser_name, 94469376953344, 40000, 4, 3, 0);

    HostMmioSpan span{trace_environment, trace_context, source_id, service_name, 0};

    REQUIRE(span.IsPending());
    REQUIRE(span.AddToSpan(mmio_r));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
  }
}

TEST_CASE("Test HostMsixSpan", "[HostMsixSpan]") {

  const uint64_t source_id = 1;
  const size_t parser_ident = 1;
  const std::string parser_name = "test";
  std::string service_name = "test-service";
  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};
  auto trace_context = std::make_shared<TraceContext>(0, 0);

  SECTION("msix followed by dma completion with id 0") {
    auto msix = std::make_shared<HostMsiX>(1967472876000, parser_ident, parser_name, 1);
    auto dma_c = std::make_shared<HostDmaC>(1967472982000, parser_ident, parser_name, 0);

    HostMsixSpan span{trace_environment, trace_context, source_id, service_name};

    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.AddToSpan(msix));
    REQUIRE(span.AddToSpan(dma_c));
    REQUIRE(span.IsComplete());
    REQUIRE_FALSE(span.IsPending());
  }

  SECTION("no msix but dma with id 0") {
    auto dma_c = std::make_shared<HostDmaC>(1967472982000, parser_ident, parser_name, 0);

    HostMsixSpan span{trace_environment, trace_context, source_id, service_name};

    REQUIRE(span.IsPending());
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE_FALSE(span.AddToSpan(dma_c));
  }

  SECTION("msix followed by dma completion with non 0 id") {
    auto msix = std::make_shared<HostMsiX>(1967472876000, parser_ident, parser_name, 1);
    auto dma_c = std::make_shared<HostDmaC>(1967471876000, parser_ident, parser_name, 94465281156144);

    HostMsixSpan span{trace_environment, trace_context, source_id, service_name};

    REQUIRE(span.AddToSpan(msix));
    REQUIRE_FALSE(span.AddToSpan(dma_c));
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.IsPending());
  }

  SECTION("msix followed by arbitrary dma") {
    auto msix = std::make_shared<HostMsiX>(1967472876000, parser_ident, parser_name, 1);
    auto dma_r = std::make_shared<HostDmaR>(1967471876000, parser_ident, parser_name, 0, 0, 0);

    HostMsixSpan span{trace_environment, trace_context, source_id, service_name};

    REQUIRE(span.AddToSpan(msix));
    REQUIRE_FALSE(span.AddToSpan(dma_r));
    REQUIRE_FALSE(span.IsComplete());
    REQUIRE(span.IsPending());
  }
}

TEST_CASE("Test netowrk spans for correctness", "[NetworkSpan]") {

  const uint64_t ident = 1;
  const uint64_t source_id = 1;
  const std::string parser_name{"NetworkSpan-test-parser"};
  const NetworkEvent::NetworkDeviceType cosim_net_dev = NetworkEvent::NetworkDeviceType::kCosimNetDevice;
  const NetworkEvent::NetworkDeviceType simple_net_dev = NetworkEvent::NetworkDeviceType::kSimpleNetDevice;
  std::string service_name = "test-service";
  const TraceEnvConfig trace_env_config = TraceEnvConfig::CreateFromYaml("tests/trace-env-config.yaml");
  TraceEnvironment trace_environment{trace_env_config};
  auto trace_context = std::make_shared<TraceContext>(0, 0);

  const auto within = NetworkEvent::EventBoundaryType::kWithinSimulator;
  const auto from = NetworkEvent::EventBoundaryType::kFromAdapter;
  const auto to = NetworkEvent::EventBoundaryType::kToAdapter;

  SECTION("valid enqueue dequeue CosimNetDeviceSpan") {
    auto enq = std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, from, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    NetDeviceSpan net_device_span{trace_environment, trace_context, source_id, service_name};
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE_FALSE(net_device_span.AddToSpan(deq));
    REQUIRE(net_device_span.AddToSpan(enq));
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE(net_device_span.AddToSpan(deq));
    REQUIRE_FALSE(net_device_span.IsPending());
    REQUIRE(net_device_span.IsComplete());
  }

  SECTION("valid enqueue drop CosimNetDeviceSpan") {
    auto enq = std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, from, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt);
    auto dro = std::make_shared<NetworkDrop>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt);
    NetDeviceSpan net_device_span{trace_environment, trace_context, source_id, service_name};
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE_FALSE(net_device_span.AddToSpan(dro));
    REQUIRE(net_device_span.AddToSpan(enq));
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE(net_device_span.AddToSpan(dro));
    REQUIRE_FALSE(net_device_span.IsPending());
    REQUIRE(net_device_span.IsComplete());
  }

  SECTION("valid enqueue dequeue SimpleNetDeviceSpan") {
    auto enq = std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98, within, std::nullopt, std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98, within, std::nullopt, std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    NetDeviceSpan net_device_span{trace_environment, trace_context, source_id, service_name};
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE_FALSE(net_device_span.AddToSpan(deq));
    REQUIRE(net_device_span.AddToSpan(enq));
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE(net_device_span.AddToSpan(deq));
    REQUIRE_FALSE(net_device_span.IsPending());
    REQUIRE(net_device_span.IsComplete());
  }

  SECTION("valid enqueue drop SimpleNetDeviceSpan") {
    auto enq = std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto dro = std::make_shared<NetworkDrop>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    NetDeviceSpan net_device_span{trace_environment, trace_context, source_id, service_name};
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE_FALSE(net_device_span.AddToSpan(dro));
    REQUIRE(net_device_span.AddToSpan(enq));
    REQUIRE(net_device_span.IsPending());
    REQUIRE_FALSE(net_device_span.IsComplete());
    REQUIRE(net_device_span.AddToSpan(dro));
    REQUIRE_FALSE(net_device_span.IsPending());
    REQUIRE(net_device_span.IsComplete());
  }

  SECTION("cannot add CosimNetDevice to SimpleNetDevice") {
    auto enq_a = std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98, from, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq_a = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    NetDeviceSpan net_device_span_a{trace_environment, trace_context, source_id, service_name};
    REQUIRE(net_device_span_a.IsPending());
    REQUIRE_FALSE(net_device_span_a.IsComplete());
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_a));
    REQUIRE(net_device_span_a.AddToSpan(enq_a));
    REQUIRE(net_device_span_a.IsPending());
    REQUIRE_FALSE(net_device_span_a.IsComplete());
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_a));
    REQUIRE(net_device_span_a.IsPending());
    REQUIRE_FALSE(net_device_span_a.IsComplete());
  }

  SECTION("cannot add non matching events") {
    auto enq_a = std::make_shared<NetworkEnqueue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, from, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq_a = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 1,  within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq_b = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, CreateEthHeader(0xc0b8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq_c = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 254, 168, 64, 1, 192, 168, 64, 2));
    auto deq_d = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, std::nullopt);
    auto deq_e = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, cosim_net_dev, 98, within, std::nullopt, std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    auto deq_f = std::make_shared<NetworkDequeue>(1947453940000, ident, parser_name, 1, 1, simple_net_dev, 98, within, CreateEthHeader(0xc0a8, 0x00, 0x00, 0x40, 0x01, 0x71, 0xb1, 0x45, 0x00, 0x00, 0x54, 0x07, 0xa4), std::nullopt, CreateIpHeader(84, 192, 168, 64, 1, 192, 168, 64, 2));
    NetDeviceSpan net_device_span_a{trace_environment, trace_context, source_id, service_name};
    REQUIRE(net_device_span_a.IsPending());
    REQUIRE_FALSE(net_device_span_a.IsComplete());
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_a));
    REQUIRE(net_device_span_a.AddToSpan(enq_a));
    REQUIRE(net_device_span_a.IsPending());
    REQUIRE_FALSE(net_device_span_a.IsComplete());
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_a));
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_b));
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_c));
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_d));
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_e));
    REQUIRE_FALSE(net_device_span_a.AddToSpan(deq_f));
    REQUIRE(net_device_span_a.IsPending());
    REQUIRE_FALSE(net_device_span_a.IsComplete());
  }
}


