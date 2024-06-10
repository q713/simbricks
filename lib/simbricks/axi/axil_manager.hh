/*
 * Copyright 2024 Max Planck Institute for Software Systems, and
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

#ifndef SIMBRICKS_AXI_AXIL_MANAGER_HH_
#define SIMBRICKS_AXI_AXIL_MANAGER_HH_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <variant>

// #define AXIL_R_DEBUG 0
// #define AXIL_W_DEBUG 0

namespace simbricks {
struct AXILOperationR {
  AXILOperationR(uint64_t addr, uint64_t req_id) : addr(addr), req_id(req_id) {
  }
  uint64_t addr;
  uint64_t req_id;
  uint64_t data = 0;
};

struct AXILOperationW {
  AXILOperationW(uint64_t addr, uint64_t req_id, uint64_t data, bool posted)
      : addr(addr), req_id(req_id), data(data), posted(posted) {
  }
  uint64_t addr;
  uint64_t req_id;
  uint64_t data;
  bool posted;
};

template <size_t BytesAddr, size_t BytesData>
class AXILManagerReadPort {
  /* address channel */
  uint8_t *const ar_addr_;
  const uint8_t &ar_ready_;
  uint8_t &ar_valid_;

  /* data channel */
  const uint8_t *const r_data_;
  uint8_t &r_ready_;
  const uint8_t &r_valid_;
  uint8_t &r_resp_;

  /* Temp values for outputs. We can't update the outputs directly in `step()`
  since that violates the semantics of non-blocking assignments. */
  uint8_t ar_valid_tmp_ = 0;
  uint64_t ar_addr_tmp_ = 0;

  /* other stuff */
  uint64_t main_time_ = 0;
  AXILOperationR *cur_op_ = nullptr;
  std::function<void()> op_done_cb_;
  bool handling_op_ = false;

 public:
  AXILManagerReadPort(uint8_t *ar_addr, const uint8_t &ar_ready,
                      uint8_t &ar_valid, const uint8_t *r_data,
                      uint8_t &r_ready, const uint8_t &r_valid, uint8_t &r_resp,
                      std::function<void()> op_done_cb)
      : ar_addr_(ar_addr),
        ar_ready_(ar_ready),
        ar_valid_(ar_valid),
        r_data_(r_data),
        r_ready_(r_ready),
        r_valid_(r_valid),
        r_resp_(r_resp),
        op_done_cb_(std::move(op_done_cb)) {
  }
  void step(uint64_t cur_ts);
  void step_apply();

  void set_op(AXILOperationR &axi_op) {
    if (cur_op_ != nullptr) {
      throw "AXILManagerReadPort::set_op() cur_op_ must be completed i.e. NULL "
      "before setting new one";
    }
    cur_op_ = &axi_op;
  }
};

template <size_t BytesAddr, size_t BytesData>
class AXILManagerWritePort {
  /* address channel */
  uint8_t *const aw_addr_;
  const uint8_t &aw_ready_;
  uint8_t &aw_valid_;

  /* data channel */
  uint8_t *const w_data_;
  const uint8_t &w_ready_;
  uint8_t &w_valid_;
  uint8_t &w_strb_;

  /* response channel */
  uint8_t &b_ready_;
  const uint8_t &b_valid_;
  const uint8_t &b_resp_;

  /* Temp values for outputs. We can't update the outputs directly in `step()`
  since that violates the semantics of non-blocking assignments. */
  uint8_t aw_valid_tmp_ = 0;
  uint64_t aw_addr_tmp_ = 0;
  uint8_t w_valid_tmp_ = 0;
  uint64_t w_data_tmp_ = 0;

  /* other stuff */
  uint64_t main_time_ = 0;
  AXILOperationW *cur_op_ = nullptr;
  std::function<void()> op_done_cb_;
  bool handling_op_ = false;

 public:
  AXILManagerWritePort(uint8_t *aw_addr, const uint8_t &aw_ready,
                       uint8_t &aw_valid, uint8_t *w_data,
                       const uint8_t &w_ready, uint8_t &w_valid,
                       uint8_t &w_strb, uint8_t &b_ready,
                       const uint8_t &b_valid, const uint8_t &b_resp,
                       std::function<void()> op_done_cb)
      : aw_addr_(aw_addr),
        aw_ready_(aw_ready),
        aw_valid_(aw_valid),
        w_data_(w_data),
        w_ready_(w_ready),
        w_valid_(w_valid),
        w_strb_(w_strb),
        b_ready_(b_ready),
        b_valid_(b_valid),
        b_resp_(b_resp),
        op_done_cb_(std::move(op_done_cb)) {
  }
  void step(uint64_t cur_ts);
  void step_apply();

  void set_op(AXILOperationW &axi_op) {
    if (cur_op_ != nullptr) {
      throw "AXILManagerReadPort::set_op() cur_op_ must be completed i.e. NULL "
      "before setting new one";
    }
    cur_op_ = &axi_op;
  }
};

template <size_t BytesAddr, size_t BytesData>
class AXILManager {
  static_assert(BytesData == 4 || BytesData == 8,
                "AXI 4 Lite standard dictates 32 or 64 bits data width");
  static_assert(BytesAddr <= 8);

  using ReadPortT = AXILManagerReadPort<BytesAddr, BytesData>;
  using WritePortT = AXILManagerWritePort<BytesAddr, BytesData>;

 private:
  ReadPortT read_port_;
  WritePortT write_port_;
  std::deque<std::variant<AXILOperationR, AXILOperationW>> pending_{};
  enum { NONE, READ, WRITE } step_on_ = NONE;

 public:
  AXILManager(uint8_t *ar_addr, const uint8_t &ar_ready, uint8_t &ar_valid,
              const uint8_t *r_data, uint8_t &r_ready, const uint8_t &r_valid,
              uint8_t &r_resp, uint8_t *aw_addr, const uint8_t &aw_ready,
              uint8_t &aw_valid, uint8_t *w_data, const uint8_t &w_ready,
              uint8_t &w_valid, uint8_t &w_strb, uint8_t &b_ready,
              const uint8_t &b_valid, const uint8_t &b_resp)
      : read_port_(ReadPortT(
            ar_addr, ar_ready, ar_valid, r_data, r_ready, r_valid, r_resp,
            std::bind(&AXILManager<BytesAddr, BytesData>::op_done, this))),
        write_port_(WritePortT(
            aw_addr, aw_ready, aw_valid, w_data, w_ready, w_valid, w_strb,
            b_ready, b_valid, b_resp,
            std::bind(&AXILManager<BytesAddr, BytesData>::op_done, this))) {
  }

  /*
    Updates the output signals based on the inputs.

    This function doesn't apply the output changes yet. This is necessary to
    properly model the semantics of non-blocking writes, i.e. the changes only
    become visible to the connected Subordinate in the next clock cycle. In
    Verilator, call this function before `eval()`. To apply the output changes,
    call `step_apply()`.
  */
  void step(uint64_t cur_ts);

  /*
    Applies the output changes. In Verilator, call this function after `eval()`.
  */
  void step_apply();

  /* issue a new request */
  void issue_read(uint64_t req_id, uint64_t addr);
  void issue_write(uint64_t req_id, uint64_t addr, uint64_t data, bool posted);

 protected:
  virtual void read_done(AXILOperationR &axi_op) = 0;
  virtual void write_done(AXILOperationW &axi_op) = 0;

 private:
  void op_done();
  void ports_set_op();
};

constexpr bool isPowerOfTwo(size_t n) {
  return n and (not(n and (n - 1)));
}

template <
    size_t DataWidthBytes = 4, size_t PacketBufSize = 2048,
    typename std::enable_if_t<DataWidthBytes >= 1 and DataWidthBytes <= 128 and
                              isPowerOfTwo(DataWidthBytes)>,
    typename std::enable_if<PacketBufSize >= 2048 and
                            isPowerOfTwo(PacketBufSize)>>
class AXISManager {
  /**
   * ACLK is a global clock signal. All signals
   * are sampled on the rising edge of ACLK
   */
  const uint8_t &aclk_;
  /**
   * ARESETn is a global reset signal.
   */
  const uint8_t &areset_n_;
  /**
   * TVALID indicates the Transmitter is driving a valid transfer. A transfer
   * takes place when both TVALID and TREADY are asserted.
   */
  const uint8_t &tvalid_;
  /**
   * TREADY indicates that a Receiver can
   * accept a transfer.
   */
  const uint8_t &tready_ = 0;
  /**
   * TDATA is the primary payload used to provide the data that is passing
   * across the interface. TDATA_WIDTH must be an integer number of bytes and
   * is recommended to be 8, 16, 32, 64, 128, 256, 512 or 1024-bits.
   */
  const uint8_t *const tdata_;

  /**
   * TSTRB is the byte qualifier that indicates whether the content of the
   * associated byte of TDATA is processed as a data byte or a position byte
   */
  const uint8_t *const tstrb_;
  constexpr size_t kStrbWidth = DataWidthBytes / 8;
  /**
   * TKEEP is the byte qualifier that indicates whether content of the
   * associated byte of TDATA is processed as part of the data stream.
   */
  const uint8_t *const tkeep_;
  constexpr size_t kKeepWidth = DataWidthBytes / 8;
  /**
   * TLAST indicates the boundary of a packet
   */
  uint8_t &tlast_ = 0;
  /**
   * TID is a data stream identifier.
   **/
  // TODO: TID Width
  const uint8_t *const tid;
  /**
   * TDEST provides routing information for the data stream.
   */
  // TODO: TDEST_WIDTH
  const uint8_t *const tdest_;
  /**
   * TUSER is a user-defined sideband information that can be transmitted along
   * the data stream. TUSER_WIDTH is recommended to be an integer multiple of
   * TDATA_WIDTH/8.
   */
  // TODO: TUSER_WIDTH
  const uint8_t *const tuser_;
  /**
   * TWAKEUP identifies any activity associated with AXI-Stream interface.
   */
  uint8_t twakeup_ = 0;
  /**
   * Packet Buffer to store the packet that is to be transmitted.
   */
  std::array<uint8_t, PacketBufSize> packet_buf_{0};
  size_t cur_packet_len_ = 0;

  bool data_transfer_can_happen() const {
    return tvalid_ and tready_;
  }

  bool isSet(const uint8_t *const bitmap, size_t index) const {
    if (index >= DataWidthBytes) {
      std::terminate();
    }
    const size_t int_pos = index / 8;
    const size_t bit_pos = index % 8;
    const uint8_t b = bitmap[int_pos];
    return static_cast<bool>(b & (1 << bit_pos));
  }

 public:
  explicit AXISManager() {
    // TODO
  }

  /**
   * SimBricks Users must override this method and implement the SimBricks
   * related message handling.
   */
  virtual void write_done() noexcept = 0;

  void step() noexcept {
    if (not data_transfer_can_happen()) {
      return;
    }

    for (size_t index = 0; index < DataWidth; index++) {
      if (not isSet(tkeep_, index)) {
        continue;
      }
      if (cur_packet_len_ >= PacketBufSize) {
        std::terminate();
      }
      packet_buf_[cur_packet_len_] = tdata_[index];
      ++cur_packet_len_;
    }

    if (static_cast<bool>(tlast_)) {
      write_done();
      cur_packet_len_ = 0;
    }
  }
  // void step_apply() noexcept {
  //   // TODO
  // }
};

/******************************************************************************/
/* Start of implementation. We need to put this into the header since code for
templated classes is only emitted at instantiation. */

/* AXILManagerReadPort */

/******************************************************************************/

template <size_t BytesAddr, size_t BytesData>
void AXILManagerReadPort<BytesAddr, BytesData>::step(uint64_t cur_ts) {
  main_time_ = cur_ts;
  /* drive these signals to constants */
  r_ready_ = 1;
  r_resp_ = 0;

  /* addr handshake complete */
  if (ar_valid_ && ar_ready_) {
    assert(cur_op_ != nullptr);
    ar_valid_tmp_ = 0;
    ar_addr_tmp_ = 0;
#if AXIL_R_DEBUG
    std::cout << main_time_
              << " AXIL R addr handshake done id=" << cur_op_->req_id << "\n";
#endif
  }

  /* data handshake complete */
  if (r_ready_ && r_valid_) {
    assert(cur_op_ != nullptr);
#if AXIL_R_DEBUG
    std::cout << main_time_
              << " AXIL R read data segment id=" << cur_op_->req_id << "\n";
#endif
    std::memcpy(&cur_op_->data, r_data_, BytesData);
    cur_op_ = nullptr;
    handling_op_ = false;
    op_done_cb_();
  }

  /* issue new read request */
  if (!handling_op_ && cur_op_ != nullptr) {
    handling_op_ = true;
    ar_addr_tmp_ = cur_op_->addr;
    ar_valid_tmp_ = 1;
#if AXIL_R_DEBUG
    std::cout << main_time_ << " AXIL R issuing new op id=" << cur_op_->req_id
              << " addr=" << cur_op_->addr << "\n";
#endif
  }
}

template <size_t BytesAddr, size_t BytesData>
void AXILManagerReadPort<BytesAddr, BytesData>::step_apply() {
  ar_valid_ = ar_valid_tmp_;
  std::memcpy(ar_addr_, &ar_addr_tmp_, BytesAddr);
}

/******************************************************************************/
/* AXILManagerWritePort */
/******************************************************************************/

template <size_t BytesAddr, size_t BytesData>
void AXILManagerWritePort<BytesAddr, BytesData>::step(uint64_t cur_ts) {
  main_time_ = cur_ts;
  /* drive these signals to constants */
  w_strb_ = 0xff;
  b_ready_ = 1;

  /* addr handshake complete */
  if (aw_valid_ && aw_ready_) {
    aw_valid_tmp_ = 0;
    aw_addr_tmp_ = 0;
#if AXIL_W_DEBUG
    std::cout << main_time_
              << " AXIL W addr handshake done id=" << cur_op_->req_id << "\n";
#endif
  }

  /* handshake for data complete */
  if (w_ready_ && w_valid_) {
#if AXIL_W_DEBUG
    std::cout << main_time_
              << " AXIL W data handshake done id=" << cur_op_->req_id << "\n";
#endif
    w_valid_tmp_ = 0;
    w_data_tmp_ = 0;
  }

  /* response handshake complete */
  if (b_ready_ && b_valid_) {
#if AXIL_W_DEBUG
    std::cout << main_time_ << " AXIL W completed id=" << cur_op_->req_id
              << "\n";
#endif
    cur_op_ = nullptr;
    handling_op_ = false;
    op_done_cb_();
  }

  /* issue new request */
  if (!handling_op_ && cur_op_ != nullptr) {
#if AXIL_W_DEBUG
    std::cout << main_time_ << " AXIL W issuing new op id=" << cur_op_->req_id
              << " addr=" << cur_op_->addr << "\n";
#endif
    handling_op_ = true;
    aw_addr_tmp_ = cur_op_->addr;
    aw_valid_tmp_ = 1;
    w_data_tmp_ = cur_op_->data;
    w_valid_tmp_ = 1;
  }
}

template <size_t BytesAddr, size_t BytesData>
void AXILManagerWritePort<BytesAddr, BytesData>::step_apply() {
  aw_valid_ = aw_valid_tmp_;
  std::memcpy(aw_addr_, &aw_addr_tmp_, BytesAddr);
  w_valid_ = w_valid_tmp_;
  std::memcpy(w_data_, &w_data_tmp_, BytesData);
}

/******************************************************************************/
/* AXILManager */
/******************************************************************************/

template <size_t BytesAddr, size_t BytesData>
void AXILManager<BytesAddr, BytesData>::step(uint64_t cur_ts) {
  if (pending_.empty()) {
    return;
  }
  step_on_ = NONE;
  auto &axi_op = pending_.front();
  if (std::holds_alternative<AXILOperationR>(axi_op)) {
    read_port_.step(cur_ts);
    step_on_ = READ;
  } else if (std::holds_alternative<AXILOperationW>(axi_op)) {
    write_port_.step(cur_ts);
    step_on_ = WRITE;
  }
}

template <size_t BytesAddr, size_t BytesData>
void AXILManager<BytesAddr, BytesData>::step_apply() {
  if (step_on_ == READ) {
    read_port_.step_apply();
  } else if (step_on_ == WRITE) {
    write_port_.step_apply();
  }
}

template <size_t BytesAddr, size_t BytesData>
void AXILManager<BytesAddr, BytesData>::issue_read(uint64_t req_id,
                                                   uint64_t addr) {
  if (addr % BytesData != 0) {
    throw "AXILManager::issue_read() addr has to be aligned to BytesData";
  }
  bool was_empty = pending_.empty();
  pending_.emplace_back(AXILOperationR{addr, req_id});
  if (was_empty) {
    ports_set_op();
  }
}

template <size_t BytesAddr, size_t BytesData>
void AXILManager<BytesAddr, BytesData>::issue_write(uint64_t req_id,
                                                    uint64_t addr,
                                                    uint64_t data,
                                                    bool posted) {
  if (addr % BytesData != 0) {
    throw "AXILManager::issue_write() addr has to be aligned to BytesData";
  }
  bool was_empty = pending_.empty();
  pending_.emplace_back(AXILOperationW{addr, req_id, data, posted});
  if (was_empty) {
    ports_set_op();
  }
}

template <size_t BytesAddr, size_t BytesData>
void AXILManager<BytesAddr, BytesData>::op_done() {
  auto &axi_op = pending_.front();
  if (std::holds_alternative<AXILOperationR>(axi_op)) {
    read_done(std::get<AXILOperationR>(axi_op));
  } else if (std::holds_alternative<AXILOperationW>(axi_op)) {
    write_done(std::get<AXILOperationW>(axi_op));
  }
  pending_.pop_front();
  ports_set_op();
}

template <size_t BytesAddr, size_t BytesData>
void AXILManager<BytesAddr, BytesData>::ports_set_op() {
  if (pending_.empty()) {
    return;
  }

  auto &axi_op = pending_.front();
  if (std::holds_alternative<AXILOperationR>(axi_op)) {
    read_port_.set_op(std::get<AXILOperationR>(axi_op));
  } else if (std::holds_alternative<AXILOperationW>(axi_op)) {
    write_port_.set_op(std::get<AXILOperationW>(axi_op));
  }
}
}  // namespace simbricks

#endif  // SIMBRICKS_AXI_AXIL_MANAGER_HH_
