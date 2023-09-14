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

/*
 HostCall: source_id=0, source_name=Gem5ClientParser, timestamp=1967474305000, pc=ffffffff812c9dcc, func=pci_msi_domain_write_msg, comp=Linuxvm-Symbols
HostCall: source_id=0, source_name=Gem5ClientParser, timestamp=1967474318750, pc=ffffffff812c9b00, func=__pci_write_msi_msg, comp=Linuxvm-Symbols <-----
HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967474400999, id=94469376954304, addr=c0400010, size=4, bar=0, offset=0
HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967474400999
HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967474526999, id=94469376954304, addr=c0400014, size=4, bar=0, offset=0
HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967474526999
HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967474652874, id=94469376954304, addr=c0400018, size=4, bar=0, offset=0
HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967474652874
HostMmioR: source_id=0, source_name=Gem5ClientParser, timestamp=1967474783625, id=94469376954304, addr=c0400018, size=4, bar=0, offset=0
 */

/*
HostCall: source_id=0, source_name=Gem5ClientParser, timestamp=1967474994250, pc=ffffffff812c8b7d, func=pci_msi_unmask_irq, comp=Linuxvm-Symbols <-------------
HostCall: source_id=0, source_name=Gem5ClientParser, timestamp=1967474996250, pc=ffffffff812c8a7c, func=pci_msix_write_vector_ctrl, comp=Linuxvm-Symbols <-----
HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967475060874, id=94469376954448, addr=c040001c, size=4, bar=0, offset=0
HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967475060874
 */


