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

#include "events/events.h"

void Event::display(std::ostream &out) {
  out << get_name() << ": source_id=" << parser_identifier_
     << ", source_name=" << parser_name_ << ", timestamp=" << timestamp_;
}

bool Event::equal(const Event &other) {
  return timestamp_ == other.timestamp_ and parser_identifier_ == other.parser_identifier_
      and parser_name_ == other.parser_name_ and type_ == other.type_ and name_ == other.name_;
}

void SimSendSync::display(std::ostream &out) {
  Event::display(out);
}

bool SimSendSync::equal(const Event &other) {
  return Event::equal(other);
}

void SimProcInEvent::display(std::ostream &out) {
  Event::display(out);
}

bool SimProcInEvent::equal(const Event &other) {
  return Event::equal(other);
}

void HostInstr::display(std::ostream &out) {
  Event::display(out);
  out << ", pc=" << std::hex << pc_;
}

bool HostInstr::equal(const Event &other) {
  if (not is_type(other, EventType::HostInstr_t)) {
    return false;
  }
  const HostInstr hinstr = static_cast<const HostInstr &>(other);
  return pc_ == hinstr.pc_ and Event::equal(hinstr);
}

void HostCall::display(std::ostream &out) {
  HostInstr::display(out);
  out << ", func=" << (func_ ? *func_ : "null");
  out << ", comp=" << (comp_ ? *comp_ : "null");
}

bool HostCall::equal(const Event &other) {
  if (not is_type(other, EventType::HostCall_t)) {
    return false;
  }
  const HostCall call = static_cast<const HostCall &>(other);
  return func_ == call.func_ and comp_ == call.comp_ and HostInstr::equal(call);
}

void HostMmioImRespPoW::display(std::ostream &out) {
  Event::display(out);
}

bool HostMmioImRespPoW::equal(const Event &other) {
  return Event::equal(other);
}

void HostIdOp::display(std::ostream &out) {
  Event::display(out);
  out << ", id=" << id_;
}

bool HostIdOp::equal(const Event &other) {
  const HostIdOp *iop = dynamic_cast<const HostIdOp *>(&other);
  if (not iop) {
    return false;
  }
  return id_ == iop->id_ and Event::equal(*iop);
}

void HostMmioCR::display(std::ostream &out) {
  HostIdOp::display(out);
}

bool HostMmioCR::equal(const Event &other) {
  return HostIdOp::equal(other);
}

void HostMmioCW::display(std::ostream &out) {
  HostIdOp::display(out);
}

bool HostMmioCW::equal(const Event &other) {
  return HostIdOp::equal(other);
}

void HostAddrSizeOp::display(std::ostream &out) {
  HostIdOp::display(out);
  out << ", addr=" << std::hex << addr_ << ", size=" << size_;
}

bool HostAddrSizeOp::equal(const Event &other) {
  const HostAddrSizeOp *addop = dynamic_cast<const HostAddrSizeOp *>(&other);
  if (not addop) {
    return false;
  }
  return addr_ == addop->addr_ and size_ == addop->size_ and HostIdOp::equal(*addop);
}

void HostMmioOp::display(std::ostream &out) {
  HostAddrSizeOp::display(out);
  out << ", bar=" << bar_ << std::hex << ", offset=" << offset_;
}

bool HostMmioOp::equal(const Event &other) {
  const HostMmioOp *mmioop = dynamic_cast<const HostMmioOp *>(&other);
  if (not mmioop) {
    return false;
  }
  return bar_ == mmioop->bar_ and offset_ == mmioop->offset_ and HostAddrSizeOp::equal(*mmioop);
}

void HostMmioR::display(std::ostream &out) {
  HostMmioOp::display(out);
}

bool HostMmioR::equal(const Event &other) {
  return HostMmioOp::equal(other);
}

void HostMmioW::display(std::ostream &out) {
  HostMmioOp::display(out);
}

bool HostMmioW::equal(const Event &other) {
  return HostMmioOp::equal(other);
}

void HostDmaC::display(std::ostream &out) {
  HostIdOp::display(out);
}

bool HostDmaC::equal(const Event &other) {
  return HostIdOp::equal(other);
}

void HostDmaR::display(std::ostream &out) {
  HostAddrSizeOp::display(out);
}

bool HostDmaR::equal(const Event &other) {
  return HostAddrSizeOp::equal(other);
}

void HostDmaW::display(std::ostream &out) {
  HostAddrSizeOp::display(out);
}

bool HostDmaW::equal(const Event &other) {
  return HostAddrSizeOp::equal(other);
}

void HostMsiX::display(std::ostream &out) {
  Event::display(out);
  out << ", vec=" << vec_;
}

bool HostMsiX::equal(const Event &other) {
  if (not is_type(other, EventType::HostMsiX_t)) {
    return false;
  }
  const HostMsiX &msi = static_cast<const HostMsiX &>(other);
  return vec_ == msi.vec_ and Event::equal(msi);
}

void HostConf::display(std::ostream &out) {
  Event::display(out);
  out << ", dev=" << dev_ << ", func=" << func_ << ", reg=" << std::hex << reg_
     << ", bytes=" << bytes_ << ", data=" << std::hex << data_;
}

bool HostConf::equal(const Event &other) {
  if (not is_type(other, EventType::HostConf_t)) {
    return false;
  }
  const HostConf &hconf = static_cast<const HostConf &>(other);
  return dev_ == hconf.dev_ and
      func_ == hconf.func_ and
      reg_ == hconf.reg_ and
      bytes_ == hconf.bytes_ and
      data_ == hconf.data_ and
      is_read_ == hconf.is_read_ and Event::equal(hconf);
}

void HostClearInt::display(std::ostream &out) {
  Event::display(out);
}

bool HostClearInt::equal(const Event &other) {
  return Event::equal(other);
}

void HostPostInt::display(std::ostream &out) {
  Event::display(out);
}

bool HostPostInt::equal(const Event &other) {
  return Event::equal(other);
}

void HostPciRW::display(std::ostream &out) {
  Event::display(out);
  out << ", offset=" << std::hex << offset_ << ", size=" << std::hex << size_;
}

bool HostPciRW::equal(const Event &other) {
  if (not is_type(other, EventType::HostPciRW_t)) {
    return false;
  }
  const HostPciRW &pci = static_cast<const HostPciRW &>(other);
  return offset_ == pci.offset_ and size_ == pci.size_ and
      is_read_ == pci.is_read_ and Event::equal(pci);
}

void NicMsix::display(std::ostream &out) {
  Event::display(out);
  out << ", vec=" << vec_;
}

bool NicMsix::equal(const Event &other) {
  if (not is_type(other, EventType::NicMsix_t)) {
    return false;
  }
  const NicMsix &msi = static_cast<const NicMsix &>(other);
  return vec_ == msi.vec_ and isX_ == msi.isX_ and Event::equal(msi);
}

void NicDma::display(std::ostream &out) {
  Event::display(out);
  out << ", id=" << std::hex << id_ << ", addr=" << std::hex << addr_
     << ", size=" << len_;
}

bool NicDma::equal(const Event &other) {
  const NicDma *dma = dynamic_cast<const NicDma *>(&other);
  if (not dma) {
    return false;
  }
  return id_ == dma->id_ and addr_ == dma->addr_
      and len_ == dma->len_ and Event::equal(*dma);
}

void SetIX::display(std::ostream &out) {
  Event::display(out);
  out << ", interrupt=" << std::hex << intr_;
}

bool SetIX::equal(const Event &other) {
  if (not is_type(other, EventType::SetIX_t)) {
    return false;
  }
  const SetIX &six = static_cast<const SetIX &>(other);
  return intr_ == six.intr_ and Event::equal(six);
}

void NicDmaI::display(std::ostream &out) {
  NicDma::display(out);
}

bool NicDmaI::equal(const Event &other) {
  return NicDma::equal(other);
}

void NicDmaEx::display(std::ostream &out) {
  NicDma::display(out);
}

bool NicDmaEx::equal(const Event &other) {
  return NicDma::equal(other);
}

void NicDmaEn::display(std::ostream &out) {
  NicDma::display(out);
}

bool NicDmaEn::equal(const Event &other) {
  return NicDma::equal(other);
}

void NicDmaCR::display(std::ostream &out) {
  NicDma::display(out);
}

bool NicDmaCR::equal(const Event &other) {
  return NicDma::equal(other);
}

void NicDmaCW::display(std::ostream &out) {
  NicDma::display(out);
}

bool NicDmaCW::equal(const Event &other) {
  return NicDma::equal(other);
}

void NicMmio::display(std::ostream &out) {
  Event::display(out);
  out << ", off=" << std::hex << off_ << ", len=" << len_ << ", val=" << std::hex
     << val_;
}

bool NicMmio::equal(const Event &other) {
  const NicMmio *mmio = dynamic_cast<const NicMmio *>(&other);
  if (not mmio) {
    return false;
  }
  return off_ == mmio->off_ and len_ == mmio->len_ and val_ == mmio->val_ and Event::equal(*mmio);
}

void NicMmioR::display(std::ostream &out) {
  NicMmio::display(out);
}

bool NicMmioR::equal(const Event &other) {
  return NicMmio::equal(other);
}

void NicMmioW::display(std::ostream &out) {
  NicMmio::display(out);
}

bool NicMmioW::equal(const Event &other) {
  return NicMmio::equal(other);
}

void NicTrx::display(std::ostream &out) {
  Event::display(out);
  out << ", len=" << len_;
}

bool NicTrx::equal(const Event &other) {
  const NicTrx *trx = dynamic_cast<const NicTrx *>(&other);
  if (not trx) {
    return false;
  }
  return len_ == trx->len_ and Event::equal(*trx);
}

void NicTx::display(std::ostream &out) {
  NicTrx::display(out);
}

bool NicTx::equal(const Event &other) {
  return NicTrx::equal(other);
}

void NicRx::display(std::ostream &out) {
  NicTrx::display(out);
  out << ", port=" << port_;
}

bool NicRx::equal(const Event &other) {
  if (not is_type(other, EventType::NicRx_t)) {
    return false;
  }
  const NicRx &rec = static_cast<const NicRx &>(other);
  return port_ == rec.port_ and NicTrx::equal(rec);
}

bool is_type(const Event &event, EventType type) {
  return event.get_type() == type;
}

bool is_type(std::shared_ptr<Event> &event_ptr, EventType type) {
  return event_ptr && event_ptr->get_type() == type;
}
