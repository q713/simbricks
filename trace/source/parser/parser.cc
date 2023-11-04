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

#define PARSER_DEBUG_ 1

#include "parser/parser.h"
#include "util/log.h"

bool LogParser::ParseTimestamp(LineHandler &line_handler, uint64_t &timestamp) {
  line_handler.TrimL();
  if (!line_handler.ParseUintTrim(10, timestamp)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s, could not parse string repr. of timestamp from line '%s'\n",
             name_.c_str(), line_handler.GetRawLine().c_str());
#endif
    return false;
  }
  return true;
}

bool LogParser::ParseAddress(LineHandler &line_handler, uint64_t &address) {
  if (!line_handler.ParseUintTrim(16, address)) {
#ifdef PARSER_DEBUG_
    DFLOGERR("%s: could not parse address from line '%s'\n",
             name_.c_str(), line_handler.GetRawLine().c_str());
#endif
    return false;
  }
  return true;
}

bool ParseMacAddress(LineHandler &line_handler,
                                std::array<unsigned char, NetworkEvent::EthernetHeader::kMacSize> &addr) {
  int index = 0;
  uint64_t byte_buf;
  do {
    byte_buf = 0;
    if (not line_handler.ParseUintTrim(16, byte_buf)) {
      return false;
    }
    if (index < NetworkEvent::EthernetHeader::kMacSize - 1 and not line_handler.ConsumeAndTrimChar(':')) {
      return false;
    }
    addr[index] = static_cast<unsigned char>(byte_buf);
    ++index;
  } while (index < NetworkEvent::EthernetHeader::kMacSize);

  return true;
}

bool ParseIpAddress(LineHandler &line_handler, uint32_t &addr) {
  // NOTE: we currently expect full ip addresses
  addr = 0;
  std::array<uint64_t, 4> octets;
  for (int index = 0; index < 4; index++) {
    if (not line_handler.ParseUintTrim(10, octets[index]) or octets[index] > 255) {
      return false;
    }
    if (index < 3 and not line_handler.ConsumeAndTrimChar('.')) {
      return false;
    }
  }
  addr = (octets[0] << 24) + (octets[1] << 16) + (octets[2] << 8) + octets[3];
  return true;
}

std::optional<NetworkEvent::EthernetHeader> TryParseEthernetHeader(LineHandler &line_handler) {
  line_handler.TrimL();
  if (not line_handler.ConsumeAndTrimTillString("EthernetHeader")) {
    return std::nullopt;
  }

  NetworkEvent::EthernetHeader header;
  if (not line_handler.ConsumeAndTrimTillString("length/type=0x")
      or not line_handler.ParseUintTrim(16, header.length_type_)) {
    return std::nullopt;
  }

  if (not line_handler.ConsumeAndTrimTillString("source=") or not ParseMacAddress(line_handler, header.src_mac_)) {
    return std::nullopt;
  }

  if (not line_handler.ConsumeAndTrimTillString("destination=") or not ParseMacAddress(line_handler, header.dst_mac_)) {
    return std::nullopt;
  }

  return header;
}

std::optional<NetworkEvent::Ipv4Header> TryParseIpHeader(LineHandler &line_handler) {
  line_handler.TrimL();
  if (not line_handler.ConsumeAndTrimTillString("Ipv4Header")) {
    return std::nullopt;
  }

  NetworkEvent::Ipv4Header header;
  if (not line_handler.ConsumeAndTrimTillString("length: ") or not line_handler.ParseUintTrim(10, header.length_)) {
    return std::nullopt;
  }

  line_handler.TrimL();
  if (not ParseIpAddress(line_handler, header.src_ip_)) {
    return std::nullopt;
  }

  line_handler.TrimL();
  if (not line_handler.ConsumeAndTrimChar('>')) {
    return std::nullopt;
  }

  line_handler.TrimL();
  if (not ParseIpAddress(line_handler, header.dst_ip_)) {
    return std::nullopt;
  }

  return header;
}
