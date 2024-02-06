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

#ifndef SIMBRICKS_TRACE_PARSER_H_
#define SIMBRICKS_TRACE_PARSER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "spdlog/spdlog.h"

#include "util/exception.h"
#include "util/componenttable.h"
#include "sync/corobelt.h"
#include "events/events.h"
#include "reader/reader.h"
#include "reader/cReader.h"
#include "env/traceEnvironment.h"
#include "analytics/timer.h"
#include "util/utils.h"

bool ParseMacAddress(LineHandler &line_handler, NetworkEvent::MacAddress &addr);

bool ParseIpAddress(LineHandler &line_handler, NetworkEvent::Ipv4 &addr);

std::optional<NetworkEvent::EthernetHeader> TryParseEthernetHeader(LineHandler &line_handler);

std::optional<NetworkEvent::ArpHeader> TryParseArpHeader(LineHandler &line_handler);

std::optional<NetworkEvent::Ipv4Header> TryParseIpHeader(LineHandler &line_handler);

class LogParser {

 protected:
  TraceEnvironment &trace_environment_;

 private:
  const std::string name_;
  const uint64_t identifier_;

 protected:
  bool ParseTimestamp(LineHandler &line_handler, uint64_t &timestamp);

  bool ParseAddress(LineHandler &line_handler, uint64_t &address);

 public:
  explicit LogParser(TraceEnvironment &trace_environment,
                     const std::string name)
      : trace_environment_(trace_environment),
        name_(name),
        identifier_(trace_environment_.GetNextParserId()) {
  };

  inline uint64_t GetIdent() const {
    return identifier_;
  }

  inline const std::string &GetName() const {
    return name_;
  }

  virtual concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) = 0;
};

class Gem5Parser : public LogParser {
  const ComponentFilter &component_table_;

 protected:
  std::shared_ptr<Event> ParseGlobalEvent(LineHandler &line_handler, uint64_t timestamp);

  concurrencpp::result<std::shared_ptr<Event>>
  ParseSystemSwitchCpus(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcPciHost(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcPciHostInterface(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcSimbricks(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSimbricksEvent(LineHandler &line_handler, uint64_t timestamp);

 public:
  explicit Gem5Parser(TraceEnvironment &trace_environment,
                      const std::string name,
                      const ComponentFilter &component_table)
      : LogParser(trace_environment, name),
        component_table_(component_table) {
  }

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

class NicBmParser : public LogParser {
 protected:
  bool ParseOffLenValComma(LineHandler &line_handler, uint64_t &off, size_t &len, uint64_t &val);

  bool ParseOpAddrLenPending(LineHandler &line_handler, uint64_t &op, uint64_t &addr, size_t &len,
                             size_t &pending, bool with_pending);

  bool ParseMacAddress(LineHandler &line_handler, uint64_t &address);

  bool ParseSyncInfo(LineHandler &line_handler, bool &sync_pcie, bool &sync_eth);

 public:
  explicit NicBmParser(TraceEnvironment &trace_environment,
                       const std::string name)
      : LogParser(trace_environment, name) {}

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

class NS3Parser : public LogParser {

  std::shared_ptr<Event> ParseNetDevice(LineHandler &line_handler,
                                        uint64_t timestamp,
                                        EventType type,
                                        int node,
                                        int device,
                                        NetworkEvent::NetworkDeviceType device_type);

 public:
  explicit NS3Parser(TraceEnvironment &trace_environment,
                     const std::string name)
      : LogParser(trace_environment, name) {}

  concurrencpp::result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

template<bool NamedPipe, size_t LineBufferSize> requires SizeLagerZero<LineBufferSize>
class BufferedEventProvider : public Producer<std::shared_ptr<Event>> {

  TraceEnvironment &trace_environment_;
  const std::string name_;
  const std::string log_file_path_;
  LogParser &log_parser_; // NOTE: only access from within FillBuffer()!!!
  //ReaderBuffer<LineBufferSize> line_handler_buffer{name_, true};
  ReaderBuffer<MultiplePagesBytes(15)> line_handler_buffer{name_};
  std::vector<std::shared_ptr<Event>> event_buffer_;
  const size_t event_buffer_size_;
  size_t cur_size_ = 0;
  size_t cur_line_index_ = 0;
  //WeakTimer &timer_;
  Timer &timer_;
  std::shared_ptr<concurrencpp::executor> background_exec_;
  std::shared_ptr<concurrencpp::executor> normal_exec_;

  NonCoroBufferedChannel<std::shared_ptr<Event>, 1000000> event_buffer_channel_;
  std::thread buffer_filller_;

  concurrencpp::result<void>
  ResetFillBuffer() {
    if (not line_handler_buffer.IsOpen()) {
      line_handler_buffer.OpenFile(log_file_path_, NamedPipe);
      throw_on_false(event_buffer_size_ > 0,
                     "event buffer have size larger 0",
                     source_loc::current());
    }

    cur_line_index_ = 0;
    cur_size_ = 0;

    while (cur_size_ < event_buffer_size_) {

      std::pair<bool, LineHandler *>
          bh_p = co_await background_exec_->submit([&] { return line_handler_buffer.NextHandler(); });
//      std::pair<bool, LineHandler *> bh_p = line_handler_buffer.NextHandler();

      if (not bh_p.first or not bh_p.second) {
        break;
      }

      LineHandler &line_handler = *bh_p.second;

      std::shared_ptr<Event> event = co_await log_parser_.ParseEvent(line_handler);
      if (event == nullptr) {
        continue;
      }

      throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());
      event_buffer_[cur_size_] = std::move(event);
      ++cur_size_;
    }

    co_return;
  }

//  concurrencpp::result<void> FillBuffer() {
//    line_handler_buffer.OpenFile(log_file_path_, NamedPipe);
//    throw_on_false(event_buffer_size_ > 0,
//                   "event buffer have size larger 0",
//                   source_loc::current());
//
//    while (line_handler_buffer.HasStillLine()) {
//
//      std::pair<bool, LineHandler *> bh_p = line_handler_buffer.NextHandler();
//
//      if (not bh_p.first or not bh_p.second) {
//        break;
//      }
//
//      LineHandler &line_handler = *bh_p.second;
//
//      std::shared_ptr<Event> event = co_await log_parser_.ParseEvent(line_handler);
//      if (event == nullptr) {
//        continue;
//      }
//
//      throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());
//      event_buffer_channel_.Push(std::move(event));
//    }
//
//    event_buffer_channel_.CloseChannel();
//    co_return;
//  }

  [[nodiscard]] bool StillBuffered() const {
    return cur_size_ > 0 and cur_line_index_ < cur_size_;
  }

//  [[nodiscard]] bool StillBuffered() {
//    return not event_buffer_channel_.Empty();
//  }

 public:
  explicit BufferedEventProvider(TraceEnvironment &trace_environment,
                                 const std::string name,
                                 const std::string log_file_path,
                                 LogParser &log_parser,
      //WeakTimer &timer_)
                                 Timer &timer_,
                                 size_t event_buffer_size)
      : Producer<std::shared_ptr<Event>>(),
        trace_environment_(trace_environment),
        name_(name),
        log_file_path_(log_file_path),
        log_parser_(log_parser),
        timer_(timer_),
        event_buffer_size_(event_buffer_size),
        background_exec_(trace_environment_.GetBackgroundPoolExecutor().get()),
        normal_exec_(trace_environment_.GetPoolExecutor().get()) {
    event_buffer_.reserve(event_buffer_size);
//    buffer_filller_ = std::thread([&] { FillBuffer().get(); });
  };

  ~BufferedEventProvider() = default;

//  ~BufferedEventProvider() {
//    buffer_filller_.join();
//  };

//  explicit operator bool() noexcept override {
//    if (not StillBuffered()) {
//      event_buffer_channel_.WaitTillValue();
//    }
//    return StillBuffered();
//  };

  concurrencpp::result<std::optional<std::shared_ptr<Event>>> produce(std::shared_ptr<concurrencpp::executor> executor) override {
    if (not StillBuffered()) {
      spdlog::debug("{} fill buffer", log_file_path_);
      co_await ResetFillBuffer();
      spdlog::debug("{} filled buffer", log_file_path_);
      if (not StillBuffered()) {
        co_return std::nullopt;
      }
    }

    size_t next = cur_line_index_;
    ++cur_line_index_;
    std::shared_ptr<Event> event = std::move(event_buffer_[next]);
    throw_if_empty(event, TraceException::kEventIsNull, source_loc::current());
    co_return event;
  }

//  concurrencpp::result<std::optional<std::shared_ptr<Event>>> produce(std::shared_ptr<concurrencpp::executor> executor) override {
//    if (not this->operator bool()) {
//      co_return std::nullopt;
//    }
//
//    auto event_opt = event_buffer_channel_.Pop();
//    if (not event_opt.has_value()) {
//      co_return std::nullopt;
//    }
//
//    co_return *event_opt;
//  }
};

#endif  // SIMBRICKS_TRACE_PARSER_H_