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

#include "util/exception.h"
#include "util/componenttable.h"
#include "sync/corobelt.h"
#include "events/events.h"
#include "reader/reader.h"
#include "env/traceEnvironment.h"

class LogParser {

  std::shared_ptr<concurrencpp::thread_pool_executor> resume_executor_;
  const std::string name_;
  const uint64_t identifier_;

 protected:
  bool ParseTimestamp(LineHandler &line_handler, uint64_t &timestamp);

  bool ParseAddress(LineHandler &line_handler, uint64_t &address);

 public:
  explicit LogParser(std::shared_ptr<concurrencpp::thread_pool_executor> resume_executor,
                     const std::string name)
      : resume_executor_(std::move(resume_executor)),
        name_(name),
        identifier_(TraceEnvironment::GetNextParserId()) {
    throw_if_empty(resume_executor_,
                   "LogParser::LogParser: resume executor is null");
  };

  inline uint64_t GetIdent() const {
    return identifier_;
  }

  inline std::string GetName() const {
    return name_;
  }

  virtual concurrencpp::lazy_result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) = 0;
};

class Gem5Parser : public LogParser {
  ComponentFilter &component_table_;

 protected:
  std::shared_ptr<Event> ParseGlobalEvent(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSystemSwitchCpus(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSystemPcPciHost(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event>
  ParseSystemPcPciHostInterface(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSystemPcSimbricks(LineHandler &line_handler, uint64_t timestamp);

  std::shared_ptr<Event> ParseSimbricksEvent(LineHandler &line_handler, uint64_t timestamp);

 public:
  explicit Gem5Parser(std::shared_ptr<concurrencpp::thread_pool_executor> resume_executor,
                      const std::string name,
                      ComponentFilter &component_table)
      : LogParser(std::move(resume_executor), name),
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
  explicit NicBmParser(std::shared_ptr<concurrencpp::thread_pool_executor> resume_executor,
                       const std::string name)
      : LogParser(std::move(resume_executor), name) {}

  concurrencpp::lazy_result<std::shared_ptr<Event>>
  ParseEvent(LineHandler &line_handler) override;
};

template<size_t LineBufferSize, size_t EventBufferSize> requires
SizeLagerZero<LineBufferSize> and SizeLagerZero<EventBufferSize>
class BufferedEventProvider : public producer<std::shared_ptr<Event>> {

  std::shared_ptr<concurrencpp::thread_pool_executor> executor_;
  std::shared_ptr<concurrencpp::thread_pool_executor> background_executor_;
  std::shared_ptr<concurrencpp::thread_executor> thread_executor_;
  const std::string name_;
  const std::string log_file_path_;
  LogParser &log_parser_; // NOTE: only access from within FillBuffer()!!!
  NonCoroBufferedChannel<std::shared_ptr<Event>, EventBufferSize> event_buffer_channel_;

  concurrencpp::result<void>
  FillBuffer() {
    ReaderBuffer<LineBufferSize> line_handler_buffer{name_, true};
    line_handler_buffer.OpenFile(log_file_path_, true);

    std::pair<bool, LineHandler *> bh_p;
    std::shared_ptr<Event> event_ptr;
    for (bh_p = line_handler_buffer.NextHandler(); bh_p.first; bh_p = line_handler_buffer.NextHandler()) {
      assert(bh_p.first);
      LineHandler &line_handler = *bh_p.second;

      event_ptr = co_await log_parser_.ParseEvent(line_handler);
      if (not event_ptr) {
        continue;
      }

      const bool could_push = event_buffer_channel_.Push(event_ptr);
      throw_on(not could_push, "BufferedEventProvider::FillBuffer: could not push event to channel");
    }

    event_buffer_channel_.CloseChannel();
  }

 public:
  explicit BufferedEventProvider(std::shared_ptr<concurrencpp::thread_pool_executor> executor,
                                 std::shared_ptr<concurrencpp::thread_pool_executor> background_executor,
                                 std::shared_ptr<concurrencpp::thread_executor> thread_executor,
                                 const std::string name,
                                 const std::string log_file_path,
                                 LogParser &log_parser)
      : producer<std::shared_ptr<Event>>(),
        executor_(std::move(executor)),
        background_executor_(std::move(background_executor)),
        thread_executor_(std::move(thread_executor)),
        log_file_path_(log_file_path),
        log_parser_(log_parser) {
    //, line_handler_buffer_(ReaderBuffer<LineBufferSize>{name, true}) {
    throw_if_empty(executor_,
                   "BufferedEventProvider::BufferedEventProvider: executor is null");
    throw_if_empty(background_executor_,
                   "BufferedEventProvider::BufferedEventProvider: background executor is null");
    throw_if_empty(thread_executor_,
                   "BufferedEventProvider::BufferedEventProvider: thread executor is null");
  };

  ~BufferedEventProvider() = default;

  concurrencpp::result<void>
  produce(std::shared_ptr<concurrencpp::executor> resume_executor,
          std::shared_ptr<CoroChannel<std::shared_ptr<Event>>> tar_chan) override {
    throw_if_empty(resume_executor, "BufferedEventProvider::process: resume executor is null");
    throw_if_empty(tar_chan, "BufferedEventProvider::process: target channel is null");

    concurrencpp::result<concurrencpp::result<void>> producer_task = thread_executor_->submit([&]() {
      return FillBuffer();
    });

    std::optional<std::shared_ptr<Event>> event_opt;
    std::shared_ptr<Event> event_ptr = nullptr;
    for (event_opt = event_buffer_channel_.Pop(); event_opt.has_value();
         event_opt = event_buffer_channel_.Pop()) {
      assert(event_opt.has_value());
      event_ptr = event_opt.value();
      const bool could_push = co_await tar_chan->Push(resume_executor, event_ptr);
      throw_on(not could_push,
               "BufferedEventProvider::process: unable to push next event to target channel");
    }

    co_await co_await producer_task;
    co_await tar_chan->CloseChannel(resume_executor);
    std::cout << "BufferedEventProvider exits" << std::endl;
    co_return;
  }
};

#endif  // SIMBRICKS_TRACE_PARSER_H_