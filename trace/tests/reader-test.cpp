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

  // 1710532120875: system.switch_cpus: T0 : 0xffffffff814cf3c2    : mov        rax, GS:[0x1ac00]
  uint64_t timestamp, addr;
  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ParseUintTrim(10, timestamp));
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_reader.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_reader.ParseUintTrim(16, addr));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  REQUIRE(1710532120875 == timestamp);
  REQUIRE(0xffffffff814cf3c2 == addr);


  // 1710532121125: system.switch_cpus: T0 : 0xffffffff814cf3cb    : cmpxchg    DS:[rdi], rdx 
  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ParseUintTrim(10, timestamp));
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_reader.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_reader.ParseUintTrim(16, addr));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  REQUIRE(1710532121125 == timestamp);
  REQUIRE(0xffffffff814cf3cb == addr);

  // 1710969526625: system.switch_cpus: T0 : 0xffffffff81088093    : add     rax, GS:[rip + 0x7ef8d4d5]
  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ParseUintTrim(10, timestamp));
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_reader.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_reader.ParseUintTrim(16, addr));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  REQUIRE(1710969526625 == timestamp);
  REQUIRE(0xffffffff81088093 == addr);

  // 1710532121125: system.switch_cpus: T0 : 0xffffffff814cf3cb. 0 :   CMPXCHG_M_R : ldst   t1, DS:[rdi] : MemRead :  D=0xfff
  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ParseUintTrim(10, timestamp));
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_reader.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_reader.ParseUintTrim(16, addr));
  REQUIRE(line_reader.ConsumeAndTrimChar('.'));
  REQUIRE(1710532121125 == timestamp);
  REQUIRE(0xffffffff814cf3cb == addr);

  // 1710532121250: system.switch_cpus: T0 : 0xffffffff814cf3cb. 1 :   CMPXCHG_M_R : sub   t0, rax, t1 : IntAlu :  D=0x0000000000
  REQUIRE(line_reader.NextLine());
  REQUIRE(line_reader.ParseUintTrim(10, timestamp));
  REQUIRE(line_reader.ConsumeAndTrimChar(':'));
  line_reader.TrimL();
  REQUIRE(line_reader.ConsumeAndTrimString("system.switch_cpus:"));
  REQUIRE(line_reader.ConsumeAndTrimTillString("0x"));
  REQUIRE(line_reader.ParseUintTrim(16, addr));
  REQUIRE(line_reader.ConsumeAndTrimChar('.'));
  REQUIRE(1710532121250 == timestamp);
  REQUIRE(0xffffffff814cf3cb == addr);

}


