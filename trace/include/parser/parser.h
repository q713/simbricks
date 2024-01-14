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

  virtual concurrencpp::lazy_result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) = 0;
};

class Gem5Parser : public LogParser {
  const ComponentFilter &component_table_;

 protected:
  std::shared_ptr<Event> ParseGlobalEvent(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSystemSwitchCpus(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSystemPcPciHost(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcPciHostInterface(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSystemPcSimbricks(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSimbricksEvent(LineHandler &line_handler, uint64_t timestamp);

 public:
  explicit Gem5Parser(TraceEnvironment &trace_environment,
                      const std::string name,
                      const ComponentFilter &component_table)
      : LogParser(trace_environment, name),
        component_table_(component_table) {
  }

  concurrencpp::lazy_result<std::shared_ptr<Event>>
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

  concurrencpp::lazy_result<std::shared_ptr<Event>>
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

  concurrencpp::lazy_result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

template<bool NamedPipe, size_t LineBufferSize> requires SizeLagerZero<LineBufferSize>
class BufferedEventProvider : public Producer<std::shared_ptr<Event>> {

  TraceEnvironment &trace_environment_;
  const std::string name_;
  const std::string log_file_path_;
  LogParser &log_parser_; // NOTE: only access from within FillBuffer()!!!
  //ReaderBuffer<LineBufferSize> line_handler_buffer{name_, true};
  ReaderBuffer<MultiplePagesBytes(8)> line_handler_buffer{name_};
  std::vector<std::shared_ptr<Event>> event_buffer_;
  const size_t event_buffer_size_;
  size_t cur_size_ = 0;
  size_t cur_line_index_ = 0;

  //WeakTimer &timer_;
  Timer &timer_;

  concurrencpp::result<void>
  ResetFillBuffer() {
    cur_line_index_ = 0;
    cur_size_ = 0;

    while (cur_size_ < event_buffer_size_) {

      std::pair<bool, LineHandler *> bh_p = line_handler_buffer.NextHandler();
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

  [[nodiscard]] bool StillBuffered() const {
    return cur_size_ > 0 and cur_line_index_ < cur_size_;
  }

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
        event_buffer_size_(event_buffer_size) {
    line_handler_buffer.OpenFile(log_file_path_, NamedPipe);
    throw_on_false(event_buffer_size_ > 0,
                   "event buffer have size larger 0",
                   source_loc::current());
    event_buffer_.reserve(event_buffer_size);
  };

  ~BufferedEventProvider() = default;

  explicit operator bool() noexcept override {
    if (not StillBuffered()) {
      ResetFillBuffer().get();
    }
    return StillBuffered();
  };

  concurrencpp::result<std::optional<std::shared_ptr<Event>>> produce(std::shared_ptr<concurrencpp::executor> executor) override {
    if (not StillBuffered()) {
      co_await ResetFillBuffer();
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
};

#if 0
template<size_t LineBufferSize, size_t EventBufferSize = 1>
requires SizeLagerZero<LineBufferSize> and SizeLagerZero<EventBufferSize>
concurrencpp::result<void> FillBuffer(concurrencpp::executor_tag,
                                      std::shared_ptr<concurrencpp::thread_pool_executor> resume_executor,
                                      const std::string &name,
                                      const std::string &log_file_path,
                                      LogParser &log_parser,
                                      CoroBoundedChannel<std::shared_ptr<Event>,
                                                         EventBufferSize> &event_buffer_channel) {
  ReaderBuffer<LineBufferSize> line_handler_buffer{name, true};
  line_handler_buffer.OpenFile(log_file_path, true);

  std::pair<bool, LineHandler *> bh_p;
  std::shared_ptr<Event> event_ptr;
  for (bh_p = line_handler_buffer.NextHandler(); bh_p.first; bh_p = line_handler_buffer.NextHandler()) {
    assert(bh_p.second);
    LineHandler &line_handler = *bh_p.second;

    //std::cout << "BufferedEventProvider::FillBuffer: got next LineHandler" << std::endl;
    event_ptr = co_await log_parser.ParseEvent(line_handler);
    if (not event_ptr) {
      continue;
    }

    const bool could_push = co_await event_buffer_channel.Push(resume_executor, event_ptr);
    throw_on_false(could_push, "BufferedEventProvider::FillBuffer: could not push event to channel",
                   source_loc::current());
  }

  co_await event_buffer_channel.CloseChannel(resume_executor);
};
#endif

#if 0
template<bool NamedPipe, size_t LineBufferSize, size_t EventBufferSize = 1> requires
SizeLagerZero<LineBufferSize> and SizeLagerZero<EventBufferSize>
class BufferedEventProvider : public producer<std::shared_ptr<Event>> {

  TraceEnvironment &trace_environment_;
  const std::string name_;
  const std::string log_file_path_;
  LogParser &log_parser_; // NOTE: only access from within FillBuffer()!!!
  NonCoroBufferedChannel<std::shared_ptr<Event>, EventBufferSize> event_buffer_channel_;
  //WeakTimer &timer_;
  Timer &timer_;
  std::list<std::shared_ptr<Event>> event_buffer_;

  concurrencpp::result<void>
  FillBuffer() {
    ReaderBuffer<LineBufferSize> line_handler_buffer{name_, true};
    line_handler_buffer.OpenFile(log_file_path_, NamedPipe);

    std::pair<bool, LineHandler *> bh_p;
    std::shared_ptr<Event> event_ptr;
    for (bh_p = line_handler_buffer.NextHandler(); bh_p.first; bh_p = line_handler_buffer.NextHandler()) {
      assert(bh_p.first);
      LineHandler &line_handler = *bh_p.second;

      event_ptr = co_await log_parser_.ParseEvent(line_handler);
      if (not event_ptr) {
        continue;
      }
      //std::cout << name_ << " got next line" << std::endl;

      const bool could_push = event_buffer_channel_.Push(event_ptr);
      throw_on(not could_push, "BufferedEventProvider::FillBuffer: could not push event to channel",
               source_loc::current());
    }

    event_buffer_channel_.CloseChannel();
  }

 public:
  explicit BufferedEventProvider(TraceEnvironment &trace_environment,
                                 const std::string name,
                                 const std::string log_file_path,
                                 LogParser &log_parser,
      //WeakTimer &timer_)
                                 Timer &timer_)
      : producer<std::shared_ptr<Event>>(),
        trace_environment_(trace_environment),
        name_(name),
        log_file_path_(log_file_path),
        log_parser_(log_parser),
        timer_(timer_) {
  };

  ~BufferedEventProvider() = default;

#if 0
  concurrencpp::result<void>
  produce(std::shared_ptr<concurrencpp::executor> resume_executor,
          std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> tar_chan) override {
    throw_if_empty(resume_executor, "BufferedEventProvider::process: resume executor is null",
                   source_loc::current());
    throw_if_empty(tar_chan, "BufferedEventProvider::process: target channel is null",
                   source_loc::current());

    ReaderBuffer<LineBufferSize> line_handler_buffer{name_, true};
    line_handler_buffer.OpenFile(log_file_path_, NamedPipe);

    std::pair<bool, LineHandler *> bh_p;
    std::shared_ptr<Event> event_ptr;
    for (bh_p = line_handler_buffer.NextHandler(); bh_p.first; bh_p = line_handler_buffer.NextHandler()) {
      assert(bh_p.first);
      LineHandler &line_handler = *bh_p.second;

      event_ptr = co_await log_parser_.ParseEvent(line_handler);
      if (not event_ptr) {
        continue;
      }

      event_buffer_.push_back(event_ptr);

      if (event_buffer_.size() < EventBufferSize) {
        continue;
      }

      while (not event_buffer_.empty()) {
        auto event = event_buffer_.front();

        const bool could_push = co_await tar_chan->Push(resume_executor, event);
        throw_on(not could_push, "BufferedEventProvider::FillBuffer: could not push event to channel",
                 source_loc::current());

        event_buffer_.pop_front();
      }
    }

    while (not event_buffer_.empty()) {
      auto event = event_buffer_.front();

      const bool could_push = co_await tar_chan->Push(resume_executor, event);
      throw_on(not could_push, "BufferedEventProvider::FillBuffer: could not push event to channel",
               source_loc::current());
    }

    co_await tar_chan->CloseChannel(resume_executor);
  }
#endif

  concurrencpp::result<void>
  produce(std::shared_ptr<concurrencpp::executor> resume_executor,
          std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> tar_chan) override {
    throw_if_empty(resume_executor, "BufferedEventProvider::process: resume executor is null",
                   source_loc::current());
    throw_if_empty(tar_chan, "BufferedEventProvider::process: target channel is null",
                   source_loc::current());

    concurrencpp::result<concurrencpp::result<void>>
        producer_task = trace_environment_.GetThreadExecutor()->submit([&]() {
      return FillBuffer();
    });

    //size_t timer_key = co_await timer_.Register(resume_executor);

    std::optional<std::shared_ptr<Event>> event_opt;
    std::shared_ptr<Event> event_ptr = nullptr;
    for (event_opt = event_buffer_channel_.Pop(); event_opt.has_value();
         event_opt = event_buffer_channel_.Pop()) {
      assert(event_opt.has_value());
      event_ptr = event_opt.value();

      //co_await timer_.MoveForward(resume_executor, timer_key, event_ptr->GetTs());
      // TODO: to enable the timer we need sync events from ns3 as well as the nicbm
      //co_await timer_.MoveForward(resume_executor, event_ptr->GetTs());

      const bool could_push = co_await tar_chan->Push(resume_executor, event_ptr);
      throw_on(not could_push,
               "BufferedEventProvider::process: unable to push next event to target channel",
               source_loc::current());
    }

    //co_await timer_.Done(resume_executor);
    co_await co_await producer_task;
    co_await tar_chan->CloseChannel(resume_executor);

    //co_await timer_.Done(resume_executor, timer_key);
    spdlog::debug("BufferedEventProvider '{}' exits", name_);
    co_return;
  }
};
#endif

#endif  // SIMBRICKS_TRACE_PARSER_H_