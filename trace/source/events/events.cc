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

void Event::Display(std::ostream &out) {
  out << GetName();
  out << ": source_id=" << parser_identifier_;
  out << ", source_name=" << parser_name_;
  out << ", timestamp=" << std::to_string(timestamp_);
}

bool Event::Equal(const Event &other) {
  return timestamp_ == other.timestamp_ and parser_identifier_ == other.parser_identifier_
      and parser_name_ == other.parser_name_ and type_ == other.type_ and name_ == other.name_;
}

void SimSendSync::Display(std::ostream &out) {
  Event::Display(out);
}

bool SimSendSync::Equal(const Event &other) {
  return Event::Equal(other);
}

void SimProcInEvent::Display(std::ostream &out) {
  Event::Display(out);
}

bool SimProcInEvent::Equal(const Event &other) {
  return Event::Equal(other);
}

void HostInstr::Display(std::ostream &out) {
  Event::Display(out);
  out << ", pc=" << std::hex << pc_;
}

bool HostInstr::Equal(const Event &other) {
  if (not IsType(other, EventType::kHostInstrT)) {
    return false;
  }
  const HostInstr hinstr = static_cast<const HostInstr &>(other);
  return pc_ == hinstr.pc_ and Event::Equal(hinstr);
}
uint64_t HostInstr::GetPc() const {
  return pc_;
}

void HostCall::Display(std::ostream &out) {
  HostInstr::Display(out);
  out << ", func=" << (func_ ? *func_ : "null");
  out << ", comp=" << (comp_ ? *comp_ : "null");
}

bool HostCall::Equal(const Event &other) {
  if (not IsType(other, EventType::kHostCallT)) {
    return false;
  }
  const HostCall call = static_cast<const HostCall &>(other);
  return func_ == call.func_ and comp_ == call.comp_ and HostInstr::Equal(call);
}
const std::string *HostCall::GetFunc() const {
  return func_;
}
const std::string *HostCall::GetComp() const {
  return comp_;
}

void HostMmioImRespPoW::Display(std::ostream &out) {
  Event::Display(out);
}

bool HostMmioImRespPoW::Equal(const Event &other) {
  return Event::Equal(other);
}

void HostIdOp::Display(std::ostream &out) {
  Event::Display(out);
  out << ", id=" << std::to_string(id_);
}

bool HostIdOp::Equal(const Event &other) {
  const HostIdOp *iop = dynamic_cast<const HostIdOp *>(&other);
  if (not iop) {
    return false;
  }
  return id_ == iop->id_ and Event::Equal(*iop);
}
uint64_t HostIdOp::GetId() const {
  return id_;
}

void HostMmioCR::Display(std::ostream &out) {
  HostIdOp::Display(out);
}

bool HostMmioCR::Equal(const Event &other) {
  return HostIdOp::Equal(other);
}

void HostMmioCW::Display(std::ostream &out) {
  HostIdOp::Display(out);
}

bool HostMmioCW::Equal(const Event &other) {
  return HostIdOp::Equal(other);
}

void HostAddrSizeOp::Display(std::ostream &out) {
  HostIdOp::Display(out);
  out << ", addr=" << std::hex << addr_;
  out << ", size=" << size_;
}

bool HostAddrSizeOp::Equal(const Event &other) {
  const HostAddrSizeOp *addop = dynamic_cast<const HostAddrSizeOp *>(&other);
  if (not addop) {
    return false;
  }
  return addr_ == addop->addr_ and size_ == addop->size_ and HostIdOp::Equal(*addop);
}

uint64_t HostAddrSizeOp::GetAddr() const {
  return addr_;
}

size_t HostAddrSizeOp::GetSize() const {
  return size_;
}

void HostMmioOp::Display(std::ostream &out) {
  HostAddrSizeOp::Display(out);
  out << ", bar=" << bar_;
  out << ", offset=" << std::hex << offset_;
}

bool HostMmioOp::Equal(const Event &other) {
  const HostMmioOp *mmioop = dynamic_cast<const HostMmioOp *>(&other);
  if (not mmioop) {
    return false;
  }
  return bar_ == mmioop->bar_ and offset_ == mmioop->offset_ and HostAddrSizeOp::Equal(*mmioop);
}

int HostMmioOp::GetBar() const {
  return bar_;
}

uint64_t HostMmioOp::GetOffset() const {
  return offset_;
}

void HostMmioR::Display(std::ostream &out) {
  HostMmioOp::Display(out);
}

bool HostMmioR::Equal(const Event &other) {
  return HostMmioOp::Equal(other);
}

void HostMmioW::Display(std::ostream &out) {
  HostMmioOp::Display(out);
}

bool HostMmioW::Equal(const Event &other) {
  return HostMmioOp::Equal(other);
}

void HostDmaC::Display(std::ostream &out) {
  HostIdOp::Display(out);
}

bool HostDmaC::Equal(const Event &other) {
  return HostIdOp::Equal(other);
}

void HostDmaR::Display(std::ostream &out) {
  HostAddrSizeOp::Display(out);
}

bool HostDmaR::Equal(const Event &other) {
  return HostAddrSizeOp::Equal(other);
}

void HostDmaW::Display(std::ostream &out) {
  HostAddrSizeOp::Display(out);
}

bool HostDmaW::Equal(const Event &other) {
  return HostAddrSizeOp::Equal(other);
}

void HostMsiX::Display(std::ostream &out) {
  Event::Display(out);
  out << ", vec=" << std::to_string(vec_);
}

bool HostMsiX::Equal(const Event &other) {
  if (not IsType(other, EventType::kHostMsiXT)) {
    return false;
  }
  const HostMsiX &msi = static_cast<const HostMsiX &>(other);
  return vec_ == msi.vec_ and Event::Equal(msi);
}

uint64_t HostMsiX::GetVec() const {
  return vec_;
}

void HostConf::Display(std::ostream &out) {
  Event::Display(out);
  out << ", dev=" << std::hex << dev_;
  out << ", func=" << std::hex << func_;
  out << ", reg=" << std::hex << reg_;
  out << ", bytes=" << bytes_;
  out << ", data=" << std::hex << data_;
}

bool HostConf::Equal(const Event &other) {
  if (not IsType(other, EventType::kHostConfT)) {
    return false;
  }
  const HostConf &hconf = static_cast<const HostConf &>(other);
  return dev_ == hconf.dev_ and
      func_ == hconf.func_ and
      reg_ == hconf.reg_ and
      bytes_ == hconf.bytes_ and
      data_ == hconf.data_ and
      is_read_ == hconf.is_read_ and Event::Equal(hconf);
}

uint64_t HostConf::GetDev() const {
  return dev_;
}

uint64_t HostConf::GetFunc() const {
  return func_;
}

uint64_t HostConf::GetReg() const {
  return reg_;
}

size_t HostConf::GetBytes() const {
  return bytes_;
}

uint64_t HostConf::GetData() const {
  return data_;
}

bool HostConf::IsRead() const {
  return is_read_;
}

void HostClearInt::Display(std::ostream &out) {
  Event::Display(out);
}

bool HostClearInt::Equal(const Event &other) {
  return Event::Equal(other);
}

void HostPostInt::Display(std::ostream &out) {
  Event::Display(out);
}

bool HostPostInt::Equal(const Event &other) {
  return Event::Equal(other);
}

void HostPciRW::Display(std::ostream &out) {
  Event::Display(out);
  out << ", offset=" << std::hex << offset_;
  out << ", size=" << size_;
}

bool HostPciRW::Equal(const Event &other) {
  if (not IsType(other, EventType::kHostPciRWT)) {
    return false;
  }
  const HostPciRW &pci = static_cast<const HostPciRW &>(other);
  return offset_ == pci.offset_ and size_ == pci.size_ and
      is_read_ == pci.is_read_ and Event::Equal(pci);
}

uint64_t HostPciRW::GetOffset() const {
  return offset_;
}

size_t HostPciRW::GetSize() const {
  return size_;
}

bool HostPciRW::IsRead() const {
  return is_read_;
}

void NicMsix::Display(std::ostream &out) {
  Event::Display(out);
  out << ", vec=" << std::to_string(vec_);
}

bool NicMsix::Equal(const Event &other) {
  if (not IsType(other, EventType::kNicMsixT)) {
    return false;
  }
  const NicMsix &msi = static_cast<const NicMsix &>(other);
  return vec_ == msi.vec_ and isX_ == msi.isX_ and Event::Equal(msi);
}

uint16_t NicMsix::GetVec() const {
  return vec_;
}

bool NicMsix::IsX() const {
  return isX_;
}

void NicDma::Display(std::ostream &out) {
  Event::Display(out);
  out << ", id=" << std::to_string(id_);
  out << ", addr=" << std::hex << addr_;
  out << ", size=" << len_;
}

bool NicDma::Equal(const Event &other) {
  const NicDma *dma = dynamic_cast<const NicDma *>(&other);
  if (not dma) {
    return false;
  }
  return id_ == dma->id_ and addr_ == dma->addr_
      and len_ == dma->len_ and Event::Equal(*dma);
}

uint64_t NicDma::GetId() const {
  return id_;
}

uint64_t NicDma::GetAddr() const {
  return addr_;
}

size_t NicDma::GetLen() const {
  return len_;
}

void SetIX::Display(std::ostream &out) {
  Event::Display(out);
  out << ", interrupt=" << std::hex << intr_;
}

bool SetIX::Equal(const Event &other) {
  if (not IsType(other, EventType::kSetIXT)) {
    return false;
  }
  const SetIX &six = static_cast<const SetIX &>(other);
  return intr_ == six.intr_ and Event::Equal(six);
}

uint64_t SetIX::GetIntr() const {
  return intr_;
}

void NicDmaI::Display(std::ostream &out) {
  NicDma::Display(out);
}

bool NicDmaI::Equal(const Event &other) {
  return NicDma::Equal(other);
}

void NicDmaEx::Display(std::ostream &out) {
  NicDma::Display(out);
}

bool NicDmaEx::Equal(const Event &other) {
  return NicDma::Equal(other);
}

void NicDmaEn::Display(std::ostream &out) {
  NicDma::Display(out);
}

bool NicDmaEn::Equal(const Event &other) {
  return NicDma::Equal(other);
}

void NicDmaCR::Display(std::ostream &out) {
  NicDma::Display(out);
}

bool NicDmaCR::Equal(const Event &other) {
  return NicDma::Equal(other);
}

void NicDmaCW::Display(std::ostream &out) {
  NicDma::Display(out);
}

bool NicDmaCW::Equal(const Event &other) {
  return NicDma::Equal(other);
}

void NicMmio::Display(std::ostream &out) {
  Event::Display(out);
  out << ", off=" << std::hex << off_;
  out << ", len=" << len_;
  out << ", val=" << std::hex << val_;
}

bool NicMmio::Equal(const Event &other) {
  const NicMmio *mmio = dynamic_cast<const NicMmio *>(&other);
  if (not mmio) {
    return false;
  }
  return off_ == mmio->off_ and len_ == mmio->len_ and val_ == mmio->val_ and Event::Equal(*mmio);
}

uint64_t NicMmio::GetOff() const {
  return off_;
}

size_t NicMmio::GetLen() const {
  return len_;
}

uint64_t NicMmio::GetVal() const {
  return val_;
}

void NicMmioR::Display(std::ostream &out) {
  NicMmio::Display(out);
}

bool NicMmioR::Equal(const Event &other) {
  return NicMmio::Equal(other);
}

bool NicMmioW::IsPosted() const {
  return posted_;
}

void NicMmioW::Display(std::ostream &out) {
  NicMmio::Display(out);
  out << ", posted=" << BoolToString(posted_);
}

bool NicMmioW::Equal(const Event &other) {
  if (not NicMmio::Equal(other)) {
    return false;
  }
  const NicMmioW *mmio = dynamic_cast<const NicMmioW *>(&other);
  if (not mmio) {
    return false;
  }
  return mmio->posted_ == posted_;
}

void NicTrx::Display(std::ostream &out) {
  Event::Display(out);
  out << ", len=" << len_;
  out << ", is_read=" << (is_read_ ? "true" : "false");
}

bool NicTrx::Equal(const Event &other) {
  const NicTrx *trx = dynamic_cast<const NicTrx *>(&other);
  if (not trx) {
    return false;
  }
  return len_ == trx->len_ and is_read_ == trx->is_read_ and Event::Equal(*trx);
}

size_t NicTrx::GetLen() const {
  return len_;
}

void NicTx::Display(std::ostream &out) {
  NicTrx::Display(out);
}

bool NicTx::Equal(const Event &other) {
  return NicTrx::Equal(other);
}

void NicRx::Display(std::ostream &out) {
  NicTrx::Display(out);
  out << ", port=" << std::to_string(port_);
}

bool NicRx::Equal(const Event &other) {
  if (not IsType(other, EventType::kNicRxT)) {
    return false;
  }
  const NicRx &rec = static_cast<const NicRx &>(other);
  return port_ == rec.port_ and NicTrx::Equal(rec);
}

int NicRx::GetPort() const {
  return port_;
}

bool IsType(const Event &event, EventType type) {
  return event.GetType() == type;
}

bool IsType(std::shared_ptr<Event> &event_ptr, EventType type) {
  return event_ptr && event_ptr->GetType() == type;
}
