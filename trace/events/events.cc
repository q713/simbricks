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

#include "events.h"

#include "parser.h"

void Event::display(std::ostream &os) {
  os << get_name() << ": source_id=" << parser_identifier_
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
  os << ", func=" << (func_ ? *func_ : "null")
     << ", comp=" << (comp_ ? *comp_ : "null");
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
  return event_ptr && event_ptr->get_type() == type;
}
