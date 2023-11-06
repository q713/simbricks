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

#include "parser/eventStreamParser.h"

bool EventStreamParser::ParseNetworkEvent(LineHandler &line_handler,
                                          int &node,
                                          int &device,
                                          NetworkEvent::NetworkDeviceType &device_type,
                                          uint64_t &payload_size,
                                          std::optional<NetworkEvent::EthernetHeader> &eth_h,
                                          std::optional<NetworkEvent::Ipv4Header> &ip_h) {
  static std::function<bool(unsigned char)> device_name_pre = [](unsigned char chara) {
    return sim_string_utils::is_alnum(chara) or chara == ':';
  };
  std::string device_name;
  if (not line_handler.ConsumeAndTrimTillString("node=") or
      not line_handler.ParseInt(node) or
      not line_handler.ConsumeAndTrimTillString("device=") or
      not line_handler.ParseInt(device) or
      not line_handler.ConsumeAndTrimTillString("device_name=") or
      not line_handler.ExtractAndSubstrUntilInto(device_name, device_name_pre) or
      not line_handler.ConsumeAndTrimTillString("payload_size=") or
      not line_handler.ParseUintTrim(10, payload_size)) {
    return false;
  }

  if (device_name == "ns3::CosimNetDevice") {
    device_type = NetworkEvent::kCosimNetDevice;
  } else if (device_name == "ns3::SimpleNetDevice") {
    device_type = NetworkEvent::kSimpleNetDevice;
  } else {
    return false;
  }

  line_handler.TrimL();
  eth_h = TryParseEthernetHeader(line_handler);

  line_handler.TrimL();
  ip_h = TryParseIpHeader(line_handler);

  return true;
}

concurrencpp::lazy_result<std::shared_ptr<Event>>
EventStreamParser::ParseEvent(LineHandler &line_handler) {
  line_handler.TrimL();
  std::function<bool(unsigned char)> pred = [](unsigned char c) {
    return c != ':';
  };
  std::string event_name = line_handler.ExtractAndSubstrUntil(pred);
  if (event_name.empty()) {
    std::cout << "could not parse event name: "
              << line_handler.GetRawLine() << '\n';
    co_return nullptr;
  }

  uint64_t ts;
  size_t parser_ident;
  std::string parser_name;
  if (not ParseIdentNameTs(line_handler, parser_ident, parser_name, ts)) {
    std::cout << "could not parse timestamp or source: "
              << line_handler.GetRawLine() << '\n';
    co_return nullptr;
  }

  std::shared_ptr<Event> event = nullptr;
  uint64_t pc = 0, id = 0, addr = 0, vec = 0, dev = 0, func = 0,
      data = 0, reg = 0, offset = 0, intr = 0,
      val = 0, payload_size;
  int bar = 0, port = 0, node, device;
  size_t len = 0, size = 0, bytes = 0;
  bool posted;
  std::string function, component;
  NetworkEvent::NetworkDeviceType device_type;
  std::optional<NetworkEvent::EthernetHeader> eth_h;
  std::optional<NetworkEvent::Ipv4Header> ip_h;

  if (event_name == "SimSendSyncSimSendSync") {
    event = std::make_shared<SimSendSync>(ts, parser_ident, parser_name);

  } else if (event_name == "SimProcInEvent") {
    event = std::make_shared<SimProcInEvent>(ts, parser_ident, parser_name);

  } else if (event_name == "HostInstr") {
    if (not line_handler.ConsumeAndTrimString(", pc=") or
        not line_handler.ParseUintTrim(16, pc)) {
      std::cout << "error parsing HostInstr" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<HostInstr>(ts, parser_ident, parser_name, pc);

  } else if (event_name == "HostCall") {
    if (not line_handler.ConsumeAndTrimString(", pc=") or
        not line_handler.ParseUintTrim(16, pc) or
        not line_handler.ConsumeAndTrimString(", func=") or
        not line_handler.ExtractAndSubstrUntilInto(
            function, sim_string_utils::is_alnum_dot_bar) or
        not line_handler.ConsumeAndTrimString(", comp=") or
        not line_handler.ExtractAndSubstrUntilInto(
            component, sim_string_utils::is_alnum_dot_bar)) {
      std::cout << "error parsing HostInstr" << '\n';
      co_return nullptr;
    }
    const std::string *func_ptr =
        trace_environment_.InternalizeAdditional(function);
    const std::string *comp =
        trace_environment_.InternalizeAdditional(component);

    event = std::make_shared<HostCall>(ts, parser_ident, parser_name, pc,
                                       func_ptr, comp);

  } else if (event_name == "HostMmioImRespPoW") {
    event =
        std::make_shared<HostMmioImRespPoW>(ts, parser_ident, parser_name);

  } else if (event_name == "HostMmioCR" or
      event_name == "HostMmioCW" or
      event_name == "HostDmaC") {
    if (not line_handler.ConsumeAndTrimString(", id=") or
        not line_handler.ParseUintTrim(10, id)) {
      std::cout << "error parsing HostMmioCR, HostMmioCW or HostDmaC"
                << '\n';
      co_return nullptr;
    }

    if (event_name == "HostMmioCR") {
      event =
          std::make_shared<HostMmioCR>(ts, parser_ident, parser_name, id);
    } else if (event_name == "HostMmioCW") {
      event =
          std::make_shared<HostMmioCW>(ts, parser_ident, parser_name, id);
    } else {
      event = std::make_shared<HostDmaC>(ts, parser_ident, parser_name, id);
    }

  } else if (event_name == "HostMmioR" or
      event_name == "HostMmioW" or
      event_name == "HostDmaR" or
      event_name == "HostDmaW") {
    if (not line_handler.ConsumeAndTrimString(", id=") or
        not line_handler.ParseUintTrim(10, id) or
        not line_handler.ConsumeAndTrimString(", addr=") or
        not line_handler.ParseUintTrim(16, addr) or
        not line_handler.ConsumeAndTrimString(", size=") or
        not line_handler.ParseUintTrim(16, size)) {
      std::cout
          << "error parsing HostMmioR, HostMmioW, HostDmaR or HostDmaW"
          << '\n';
      co_return nullptr;
    }

    if (event_name == "HostMmioR" or
        event_name == "HostMmioW") {
      if (not line_handler.ConsumeAndTrimString(", bar=") or
          not line_handler.ParseInt(bar) or
          not line_handler.ConsumeAndTrimString(", offset=") or
          not line_handler.ParseUintTrim(16, offset)) {
        std::cout
            << "error parsing HostMmioR, HostMmioW bar or offset"
            << '\n';
        co_return nullptr;
      }

      if (event_name == "HostMmioW") {
        if (not line_handler.ConsumeAndTrimString(", posted=") or
            not line_handler.ParseBoolFromStringRepr(posted)) {
          std::cout << "error parsing HostMmioW posted" << '\n';
          co_return nullptr;
        }
        event = std::make_shared<HostMmioW>(ts, parser_ident, parser_name,
                                            id, addr, size, bar, offset, posted);
      } else {
        event = std::make_shared<HostMmioR>(ts, parser_ident, parser_name,
                                            id, addr, size, bar, offset);
      }
    } else if (event_name == "HostDmaR") {
      event = std::make_shared<HostDmaR>(ts, parser_ident, parser_name, id,
                                         addr, size);
    } else {
      event = std::make_shared<HostDmaW>(ts, parser_ident, parser_name, id,
                                         addr, size);
    }

  } else if (event_name == "HostMsiX") {
    if (not line_handler.ConsumeAndTrimString(", vec=") or
        not line_handler.ParseUintTrim(10, vec)) {
      std::cout << "error parsing HostMsiX" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<HostMsiX>(ts, parser_ident, parser_name, vec);

  } else if (event_name == "HostConfRead" or
      event_name == "HostConfWrite") {
    if (not line_handler.ConsumeAndTrimString(", dev=") or
        not line_handler.ParseUintTrim(16, dev) or
        not line_handler.ConsumeAndTrimString(", func=") or
        not line_handler.ParseUintTrim(16, func) or
        not line_handler.ConsumeAndTrimString(", reg=") or
        not line_handler.ParseUintTrim(16, reg) or
        not line_handler.ConsumeAndTrimString(", bytes=") or
        not line_handler.ParseUintTrim(10, bytes) or
        not line_handler.ConsumeAndTrimString(", data=") or
        not line_handler.ParseUintTrim(16, data)) {
      std::cout << "error parsing HostConfRead or HostConfWrite"
                << '\n';
      co_return nullptr;
    }

    if (event_name == "HostConfRead") {
      event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                         func, reg, bytes, data, true);
    } else {
      event = std::make_shared<HostConf>(ts, parser_ident, parser_name, dev,
                                         func, reg, bytes, data, false);
    }

  } else if (event_name == "HostClearInt") {
    event = std::make_shared<HostClearInt>(ts, parser_ident, parser_name);

  } else if (event_name == "HostPostInt") {
    event = std::make_shared<HostPostInt>(ts, parser_ident, parser_name);

  } else if (event_name == "HostPciR" or
      event_name == "HostPciW") {
    if (not line_handler.ConsumeAndTrimString(", offset=") or
        not line_handler.ParseUintTrim(16, offset) or
        not line_handler.ConsumeAndTrimString(", size=") or
        not line_handler.ParseUintTrim(10, size)) {
      std::cout << "error parsing HostPciR or HostPciW" << '\n';
      co_return nullptr;
    }

    if (event_name == "HostPciR") {
      event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                          offset, size, true);
    } else {
      event = std::make_shared<HostPciRW>(ts, parser_ident, parser_name,
                                          offset, size, false);
    }

  } else if (event_name == "NicMsix" or
      event_name == "NicMsi") {
    if (not line_handler.ConsumeAndTrimString(", vec=") or
        not line_handler.ParseUintTrim(10, vec)) {
      std::cout << "error parsing NicMsix" << '\n';
      co_return nullptr;
    }

    if (event_name == "NicMsix") {
      event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                        true);
    } else {
      event = std::make_shared<NicMsix>(ts, parser_ident, parser_name, vec,
                                        false);
    }

  } else if (event_name == "SetIX") {
    if (not line_handler.ConsumeAndTrimString(", interrupt=") or
        not line_handler.ParseUintTrim(16, intr)) {
      std::cout << "error parsing NicMsix" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<SetIX>(ts, parser_ident, parser_name, intr);

  } else if (event_name == "NicDmaI" or
      event_name == "NicDmaEx" or
      event_name == "NicDmaEn" or
      event_name == "NicDmaCR" or
      event_name == "NicDmaCW") {
    if (not line_handler.ConsumeAndTrimString(", id=") or
        not line_handler.ParseUintTrim(10, id) or
        not line_handler.ConsumeAndTrimString(", addr=") or
        not line_handler.ParseUintTrim(16, addr) or
        not line_handler.ConsumeAndTrimString(", size=") or
        not line_handler.ParseUintTrim(16, len)) {
      std::cout << "error parsing NicDmaI, NicDmaEx, NicDmaEn, NicDmaCR or "
                   "NicDmaCW"
                << '\n';
      co_return nullptr;
    }

    if (event_name == "NicDmaI") {
      event = std::make_shared<NicDmaI>(ts, parser_ident, parser_name, id,
                                        addr, len);
    } else if (event_name == "NicDmaEx") {
      event = std::make_shared<NicDmaEx>(ts, parser_ident, parser_name, id,
                                         addr, len);
    } else if (event_name == "NicDmaEn") {
      event = std::make_shared<NicDmaEn>(ts, parser_ident, parser_name, id,
                                         addr, len);
    } else if (event_name == "NicDmaCW") {
      event = std::make_shared<NicDmaCW>(ts, parser_ident, parser_name, id,
                                         addr, len);
    } else {
      event = std::make_shared<NicDmaCR>(ts, parser_ident, parser_name, id,
                                         addr, len);
    }

  } else if (event_name == "NicMmioR" or
      event_name == "NicMmioW") {
    if (not line_handler.ConsumeAndTrimString(", off=") or
        not line_handler.ParseUintTrim(16, offset) or
        not line_handler.ConsumeAndTrimString(", len=") or
        not line_handler.ParseUintTrim(16, len) or
        not line_handler.ConsumeAndTrimString(", val=") or
        not line_handler.ParseUintTrim(16, val)) {
      std::cout << "error parsing NicMmioR or NicMmioW: "
                << line_handler.GetRawLine() << '\n';
      co_return nullptr;
    }

    if (event_name == "NicMmioR") {
      event = std::make_shared<NicMmioR>(ts, parser_ident, parser_name,
                                         offset, len, val);
    } else {
      if (not line_handler.ConsumeAndTrimString(", posted=") or
          not line_handler.ParseBoolFromStringRepr(posted)) {
        std::cout << "error parsing NicMmioW: "
                  << line_handler.GetRawLine() << '\n';
        co_return nullptr;
      }
      event = std::make_shared<NicMmioW>(ts, parser_ident, parser_name,
                                         offset, len, val, posted);
    }

  } else if (event_name == "NicTx") {
    if (not line_handler.ConsumeAndTrimString(", len=") or
        not line_handler.ParseUintTrim(16, len)) {
      std::cout << "error parsing NicTx" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<NicTx>(ts, parser_ident, parser_name, len);

  } else if (event_name == "NicRx") {
    if (not line_handler.ConsumeAndTrimString(", len=") or
        not line_handler.ParseUintTrim(16, len) or
        not line_handler.ConsumeAndTrimString(", is_read=true") or
        not line_handler.ConsumeAndTrimString(", port=") or
        not line_handler.ParseInt(port)) {
      std::cout << "error parsing NicRx" << '\n';
      co_return nullptr;
    }
    event = std::make_shared<NicRx>(ts, parser_ident,
                                    parser_name, len, addr);
  } else if (event_name == "NetworkEnqueue") {
    if (not ParseNetworkEvent(line_handler, node, device, device_type, payload_size, eth_h, ip_h)) {
      co_return nullptr;
    }
    co_return create_shared<NetworkEnqueue>("netowrk event null",
                                            ts,
                                            parser_ident,
                                            parser_name,
                                            node,
                                            device,
                                            device_type,
                                            payload_size,
                                            eth_h,
                                            ip_h);
  } else if (event_name == "NetworkDequeue") {
    if (not ParseNetworkEvent(line_handler, node, device, device_type, payload_size, eth_h, ip_h)) {
      co_return nullptr;
    }
    co_return create_shared<NetworkDequeue>("netowrk event null",
                                            ts,
                                            parser_ident,
                                            parser_name,
                                            node,
                                            device,
                                            device_type,
                                            payload_size,
                                            eth_h,
                                            ip_h);
  } else if (event_name == "NetworkDrop") {
    if (not ParseNetworkEvent(line_handler, node, device, device_type, payload_size, eth_h, ip_h)) {
      co_return nullptr;
    }
    co_return create_shared<NetworkDrop>("netowrk event null",
                                         ts,
                                         parser_ident,
                                         parser_name,
                                         node,
                                         device,
                                         device_type,
                                         payload_size,
                                         eth_h,
                                         ip_h);
  } else {
    std::cout << "unknown event found, it will be skipped" << '\n';
    co_return nullptr;
  }

  throw_if_empty(event, "event stream parser must have an event when returning an event",
                 source_loc::current());
  co_return event;
}
