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

#include "trace/events/events.h"

#include "trace/parser/parser.h"

void Event::display(std::ostream &os) {
  os << getName() << ": source_id=" << parser_identifier_
     << ", source_name=" << parser_name_ << ", timestamp=" << timestamp_;
}

void SimSendSync::display(std::ostream &os) {
  Event::display(os);
}

void SimProcInEvent::display(std::ostream &os) {
  Event::display(os);
}

void HostInstr::display(std::ostream &os) {
  Event::display(os);
  os << ", pc=" << std::hex << pc_;
}

void HostCall::display(std::ostream &os) {
  HostInstr::display(os);
  os << ", func=" << func_ << ", comp=" << comp_;
}

void HostMmioImRespPoW::display(std::ostream &os) {
  Event::display(os);
}

void HostIdOp::display(std::ostream &os) {
  Event::display(os);
  os << ", id=" << id_;
}

void HostMmioCR::display(std::ostream &os) {
  HostIdOp::display(os);
}

void HostMmioCW::display(std::ostream &os) {
  HostIdOp::display(os);
}

void HostAddrSizeOp::display(std::ostream &os) {
  HostIdOp::display(os);
  os << ", addr=" << std::hex << addr_ << ", size=" << size_;
}

void HostMmioOp::display(std::ostream &os) {
  HostAddrSizeOp::display(os);
  os << ", bar=" << bar_ << std::hex << ", offset=" << offset_;
}

void HostMmioR::display(std::ostream &os) {
  HostMmioOp::display(os);
}

void HostMmioW::display(std::ostream &os) {
  HostMmioOp::display(os);
}

void HostDmaC::display(std::ostream &os) {
  HostIdOp::display(os);
}

void HostDmaR::display(std::ostream &os) {
  HostAddrSizeOp::display(os);
}

void HostDmaW::display(std::ostream &os) {
  HostAddrSizeOp::display(os);
}

void HostMsiX::display(std::ostream &os) {
  Event::display(os);
  os << ", vec=" << vec_;
}

void HostConf::display(std::ostream &os) {
  Event::display(os);
  os << ", dev=" << dev_ << ", func=" << func_ << ", reg=" << std::hex << reg_
     << ", bytes=" << bytes_ << ", data=" << std::hex << data_;
}

void HostClearInt::display(std::ostream &os) {
  Event::display(os);
}

void HostPostInt::display(std::ostream &os) {
  Event::display(os);
}

void HostPciRW::display(std::ostream &os) {
  Event::display(os);
  os << ", offset=" << std::hex << offset_ << ", size=" << std::hex << size_;
}

void NicMsix::display(std::ostream &os) {
  Event::display(os);
  os << ", vec=" << vec_;
}

void NicDma::display(std::ostream &os) {
  Event::display(os);
  os << ", id=" << std::hex << id_ << ", addr=" << std::hex << addr_
     << ", size=" << len_;
}

void SetIX::display(std::ostream &os) {
  Event::display(os);
  os << ", interrupt=" << std::hex << intr_;
}

void NicDmaI::display(std::ostream &os) {
  NicDma::display(os);
}

void NicDmaEx::display(std::ostream &os) {
  NicDma::display(os);
}

void NicDmaEn::display(std::ostream &os) {
  NicDma::display(os);
}

void NicDmaCR::display(std::ostream &os) {
  NicDma::display(os);
}
void NicDmaCW::display(std::ostream &os) {
  NicDma::display(os);
}

void NicMmio::display(std::ostream &os) {
  Event::display(os);
  os << ", off=" << std::hex << off_ << ", len=" << len_ << ", val=" << std::hex
     << val_;
}

void NicMmioR::display(std::ostream &os) {
  NicMmio::display(os);
}

void NicMmioW::display(std::ostream &os) {
  NicMmio::display(os);
}

void NicTrx::display(std::ostream &os) {
  Event::display(os);
  os << ", len=" << len_;
}

void NicTx::display(std::ostream &os) {
  NicTrx::display(os);
}

void NicRx::display(std::ostream &os) {
  NicTrx::display(os);
  os << ", port=" << port_;
};

bool is_type(std::shared_ptr<Event> event_ptr, EventType type) {
  return event_ptr && event_ptr->getType() == type;
}

bool is_host_issued_mmio_event(std::shared_ptr<Event> event_ptr) {
  const static std::set<EventType> mmio_events{EventType::HostMmioR_t,
                                               EventType::HostMmioW_t,
                                               EventType::HostMmioImRespPoW_t};

  return event_ptr && mmio_events.contains(event_ptr->getType());
}

bool is_host_received_mmio_event(std::shared_ptr<Event> event_ptr) {
  const static std::set<EventType> mmio_events{EventType::HostMmioCR_t,
                                               EventType::HostMmioCW_t};

  return event_ptr && mmio_events.contains(event_ptr->getType());
}

bool is_host_mmio_event(std::shared_ptr<Event> event_ptr) {
  return is_host_issued_mmio_event(event_ptr) or
         is_host_received_mmio_event(event_ptr);
}

bool is_host_event(std::shared_ptr<Event> event_ptr) {
  const static std::set<EventType> host_events{
      EventType::HostInstr_t,         EventType::HostCall_t,
      EventType::HostMmioImRespPoW_t, EventType::HostIdOp_t,
      EventType::HostMmioCR_t,        EventType::HostMmioCW_t,
      EventType::HostAddrSizeOp_t,    EventType::HostMmioR_t,
      EventType::HostMmioW_t,         EventType::HostDmaC_t,
      EventType::HostDmaR_t,          EventType::HostDmaW_t,
      EventType::HostMsiX_t,          EventType::HostConf_t,
      EventType::HostClearInt_t,      EventType::HostPostInt_t,
      EventType::HostPciRW_t,
  };

  return event_ptr && host_events.contains(event_ptr->getType());
}

bool is_nic_event(std::shared_ptr<Event> event_ptr) {
  const static std::set<EventType> nic_events{
      EventType::NicMsix_t,  EventType::NicDma_t,   EventType::SetIX_t,
      EventType::NicDmaI_t,  EventType::NicDmaEx_t, EventType::NicDmaEn_t,
      EventType::NicDmaCR_t, EventType::NicDmaCW_t, EventType::NicMmio_t,
      EventType::NicMmioR_t, EventType::NicMmioW_t, EventType::NicTrx_t,
      EventType::NicTx_t,    EventType::NicRx_t};

  return event_ptr && nic_events.contains(event_ptr->getType());
}
