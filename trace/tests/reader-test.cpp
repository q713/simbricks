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

#include "reader/reader.h"

TEST_CASE("Test LineReader", "[LineReader]") {

  LineReader line_reader;
  int int_target;
  uint64_t hex_target;

  REQUIRE(line_reader.OpenFile("tests/line-reader-test-files/simple.txt"));

  REQUIRE(line_reader.NextLine());
  line_reader.ParseInt(int_target);
  REQUIRE(int_target == 10);
  REQUIRE(line_reader.ConsumeAndTrimChar(' '));
  REQUIRE(line_reader.ConsumeAndTrimString("Hallo"));
  REQUIRE(line_reader.ConsumeAndTrimChar(' '));
  line_reader.ParseInt(int_target);
  REQUIRE(int_target == 327846378);

  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_reader.ParseUintTrim(16, hex_target));
  REQUIRE(hex_target == 0x23645);

  REQUIRE(line_reader.NextLine());
  REQUIRE_FALSE(line_reader.ConsumeAndTrimTillString("ks"));

  REQUIRE(line_reader.NextLine());
  REQUIRE_FALSE(line_reader.IsEmpty());
  REQUIRE(line_reader.ConsumeAndTrimString("Rathaus"));

  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ConsumeAndTrimTillString("Rathaus"));

  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ConsumeAndTrimTillString("Rathaus"));

  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ConsumeAndTrimTillString("Rathaus"));

  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ConsumeAndTrimTillString("Rathaus"));

}


