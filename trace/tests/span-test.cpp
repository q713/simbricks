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

  /*
  HostMmioR: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374, id=94469376773312, addr=c0108000, size=4, bar=0, offset=0
  HostMmioCR: source_id=0, source_name=Gem5ClientParser, timestamp=1967469841374, id=94469376773312
  */
  SECTION("normal mmio read") {
    auto mmio_r = std::make_shared<HostMmioR>(1967468841374, parser_ident,
                                              parser_name, 94469376773312, 108000, 4, 0, 0);
    auto mmio_cr = std::make_shared<HostMmioCR>(1967469841374, parser_ident, parser_name, 94469376773312);

    HostMmioSpan span{source_id, false};

    REQUIRE(span.is_pending());
    REQUIRE(span.add_to_span(mmio_r));
    REQUIRE(span.add_to_span(mmio_cr));
    REQUIRE(span.is_complete());
    REQUIRE_FALSE(span.is_pending());
  }

  /*
  HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374, id=94469376773312, addr=c0108000, size=4, bar=0, offset=0
  HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967468841374
  HostMmioCW: source_id=0, source_name=Gem5ClientParser, timestamp=1967469841374, id=94469376773312
  */
  SECTION("normal mmio write") {
    auto mmio_w = std::make_shared<HostMmioW>(1967468841374, parser_ident,
                                              parser_name, 94469376773312, 108000, 4, 0, 0);
    auto mmio_imr = std::make_shared<HostMmioImRespPoW>(1967468841374, parser_ident, parser_name);
    auto mmio_cw = std::make_shared<HostMmioCW>(1967469841374, parser_ident, parser_name, 94469376773312);

    HostMmioSpan span{source_id, false};

    REQUIRE(span.is_pending());
    REQUIRE(span.add_to_span(mmio_w));
    REQUIRE(span.add_to_span(mmio_imr));
    REQUIRE(span.is_pending());
    REQUIRE_FALSE(span.is_complete());
    REQUIRE(span.add_to_span(mmio_cw));
    REQUIRE(span.is_complete());
    REQUIRE_FALSE(span.is_pending());
  }

  /*
  HostCall: source_id=0, source_name=Gem5ClientParser, timestamp=1967473336375, pc=ffffffff812c8a7c, func=pci_msix_write_vector_ctrl, comp=Linuxvm-Symbols <-----
  HostMmioW: source_id=0, source_name=Gem5ClientParser, timestamp=1967473406749, id=94469376953344, addr=c040001c, size=4, bar=0, offset=0
  HostMmioImRespPoW: source_id=0, source_name=Gem5ClientParser, timestamp=1967473406749
  HostMmioR: source_id=0, source_name=Gem5ClientParser, timestamp=1967473531624, id=94469376953344, addr=c0400000, size=4, bar=0, offset=0
   */
  SECTION("mmio write after pci with read to get write through") {
    auto mmio_w = std::make_shared<HostMmioW>(1967473406749, parser_ident,
                                              parser_name, 94469376953344, 40001, 4, 0, 0);
    auto mmio_imr = std::make_shared<HostMmioImRespPoW>(1967473406749, parser_ident, parser_name);
    auto mmio_r = std::make_shared<HostMmioR>(1967473531624, parser_ident,
                                              parser_name, 94469376953344, 40000, 4, 0, 0);

    HostMmioSpan span{source_id, true};

    REQUIRE(span.is_pending());
    REQUIRE(span.add_to_span(mmio_w));
    REQUIRE(span.add_to_span(mmio_imr));
    REQUIRE(span.add_to_span(mmio_r));
    REQUIRE(span.is_complete());
    REQUIRE_FALSE(span.is_pending());
  }
}

TEST_CASE("Test HostMsixSpan", "[HostMsixSpan]") {

  const uint64_t source_id = 1;
  const size_t parser_ident = 1;
  const std::string parser_name = "test";

  SECTION("msix followed by dma completion with id 0") {
    auto msix = std::make_shared<HostMsiX>(1967472876000, parser_ident, parser_name, 1);
    auto dma_c = std::make_shared<HostDmaC>(1967472982000, parser_ident, parser_name, 0);

    HostMsixSpan span{source_id};

    REQUIRE(span.is_pending());
    REQUIRE_FALSE(span.is_complete());
    REQUIRE(span.add_to_span(msix));
    REQUIRE(span.add_to_span(dma_c));
    REQUIRE(span.is_complete());
    REQUIRE_FALSE(span.is_pending());
  }

  SECTION("no msix but dma with id 0") {
    auto dma_c = std::make_shared<HostDmaC>(1967472982000, parser_ident, parser_name, 0);

    HostMsixSpan span{source_id};

    REQUIRE(span.is_pending());
    REQUIRE_FALSE(span.is_complete());
    REQUIRE_FALSE(span.add_to_span(dma_c));
  }

  SECTION("msix followed by dma completion with non 0 id") {
    auto msix = std::make_shared<HostMsiX>(1967472876000, parser_ident, parser_name, 1);
    auto dma_c = std::make_shared<HostDmaC>(1967471876000, parser_ident, parser_name, 94465281156144);

    HostMsixSpan span{source_id};

    REQUIRE(span.add_to_span(msix));
    REQUIRE_FALSE(span.add_to_span(dma_c));
    REQUIRE_FALSE(span.is_complete());
    REQUIRE(span.is_pending());
  }

  SECTION("msix followed by arbitrary dma") {
    auto msix = std::make_shared<HostMsiX>(1967472876000, parser_ident, parser_name, 1);
    auto dma_r = std::make_shared<HostDmaR>(1967471876000, parser_ident, parser_name, 0, 0, 0);

    HostMsixSpan span{source_id};

    REQUIRE(span.add_to_span(msix));
    REQUIRE_FALSE(span.add_to_span(dma_r));
    REQUIRE_FALSE(span.is_complete());
    REQUIRE(span.is_pending());
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


