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

#ifndef SIMBRICKS_TRACE_EVENTS_H_
#define SIMBRICKS_TRACE_EVENTS_H_

#pragma once

#include <ostream>
#include <string>
#include <cstdint>

class Event {
 public:
  uint64_t timestamp_;
  std::string instr_;

  explicit Event(uint64_t t, std::string i) : timestamp_(t), instr_(std::move(i)) {}

  friend std::ostream& operator<<(std::ostream& os, const Event &e);
};

std::ostream& operator<<(std::ostream &os, const Event &e) {
  os << "Event(timestamp=" << e.timestamp_ << ", instruction=" << e.instr_ << ")";
  return os;
}

auto event_time_comperator = [](const Event &e1, const Event &e2) {
    return e1.timestamp_ < e2.timestamp_;
};

#endif  // SIMBRICKS_TRACE_EVENTS_H_