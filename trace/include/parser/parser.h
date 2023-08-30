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
#include "corobelt/corobelt.h"
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

template<typename ValueType, size_t BufferSize> requires SizeLagerZero<BufferSize>
class NonCoroBufferedChannel {
  std::mutex chan_numtex_;
  std::condition_variable chan_cond_var_;

  bool closed_ = false;
  bool poisened_ = false;
  std::vector<ValueType> buffer_{BufferSize};
  size_t read_index_ = 0;
  size_t write_index_ = 0;
  size_t size_ = 0;

  // NOTE: the lock must be held when calling this method
  void perform_write(ValueType value) {
    assert(this->size_ < BufferSize and "the channel should not be full here");
    assert(write_index_ < BufferSize and "cannot write out of bound");
    assert(write_index_ >= 0 and "cannot write out of bound");
    buffer_[write_index_] = std::move(value);
    write_index_ = (write_index_ + 1) % BufferSize;
    ++(this->size_);
  }

  // NOTE: the lock must be held when calling this method
  ValueType perform_read() {
    assert(this->size_ > 0 and "the channel should not be empty here");
    assert(read_index_ < BufferSize and "cannot read out of bound");
    assert(read_index_ >= 0 and "cannot read out of bound");
    auto result = std::move(buffer_[read_index_]);
    read_index_ = (read_index_ + 1) % BufferSize;
    --(this->size_);
    return std::move(result);
  }

 public:

  NonCoroBufferedChannel() = default;

  NonCoroBufferedChannel(const NonCoroBufferedChannel<ValueType, BufferSize> &) = delete;

  NonCoroBufferedChannel(NonCoroBufferedChannel<ValueType, BufferSize>
                         &&) = delete;

  NonCoroBufferedChannel<ValueType, BufferSize> &operator=(
      const NonCoroBufferedChannel<ValueType, BufferSize> &) noexcept = delete;

  NonCoroBufferedChannel<ValueType, BufferSize> &operator=(
      NonCoroBufferedChannel<ValueType, BufferSize> &&) noexcept = delete;

  bool Empty() {
    std::lock_guard<std::mutex> guard{chan_numtex_};
    return size_ == 0;
  }

  size_t GetSize() {
    std::lock_guard<std::mutex> guard{chan_numtex_};
    return size_;
  }

  void CloseChannel() {
    {
      std::lock_guard<std::mutex> guard{chan_numtex_};
      closed_ = true;
    }
    chan_cond_var_.notify_all();
  }

  void PoisenChannel() {
    {
      std::lock_guard<std::mutex> guard{chan_numtex_};
      poisened_ = true;
    }
    chan_cond_var_.notify_all();
  }

  // returns false if channel is closed or poisened
  bool Push(ValueType value) {
    {
      std::unique_lock lock{chan_numtex_};
      chan_cond_var_.wait(lock, [this] {
        return this->closed_ or this->poisened_ or this->size_ < BufferSize;
      });
      if (closed_ or poisened_) {
        chan_cond_var_.notify_all();
        return false;
      }
      assert(not closed_ and "channel should not be closed here");
      assert(not poisened_ and "channel should not be poisened here");

      perform_write(std::move(value));
    }

    chan_cond_var_.notify_all();
    return true;
  }

  // returns false if channel is closed or poisened
  bool TryPush(ValueType value) {
    {
      std::lock_guard<std::mutex> guard{chan_numtex_};
      if (closed_ or poisened_ or size_ >= BufferSize) {
        co_return false;
      }
      assert(not closed_ and "channel should not be closed here");
      assert(not poisened_ and "channel should not be poisened here");
      perform_write(std::move(value));
    }
    chan_cond_var_.notify_all();
    co_return true;
  }

  // returns empty optional in case channel is poisened or empty
  std::optional<ValueType> Pop() {
    std::unique_lock lock{chan_numtex_};
    chan_cond_var_.wait(lock, [this] {
      return poisened_ || closed_ || size_ > 0;
    });
    if (poisened_) {
      chan_cond_var_.notify_all();
      return std::nullopt;
    }
    assert(not poisened_ and "channel should not be poisened here");
    if (size_ == 0) {
      lock.unlock();
      chan_cond_var_.notify_all();
      return std::nullopt;
    }
    assert(size_ > 0 and "trying to read from empty channel");
    auto result = perform_read();
    lock.unlock();
    chan_cond_var_.notify_all();
    return result;
  }

  std::optional<ValueType> TryPop() {
    std::unique_lock lock{chan_numtex_};
    if (this->poisened_ or this->size_ == 0) {
      lock.unlock();
      chan_cond_var_.notify_all();
      return std::nullopt;
    }
    auto result = perform_read();
    lock.unlock();
    chan_cond_var_.notify_all();
    return std::move(result);
  }

  std::optional<ValueType> TryPopOnTrue(std::function<bool(ValueType &)> &predicate) {
    std::unique_lock lock{chan_numtex_};
    if (this->poisened_ or this->size_ == 0) {
      lock.unlock();
      chan_cond_var_.notify_all();
      return std::nullopt;
    }
    assert(read_index_ < BufferSize and "cannot read out of bound");
    assert(read_index_ >= 0 and "cannot read out of bound");
    ValueType &value = buffer_[read_index_];
    if (not predicate(value)) {
      return std::nullopt;
    }
    auto result = perform_read();
    lock.unlock();
    chan_cond_var_.notify_all();
    return std::move(result);
  }
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
#if 0
  std::vector<std::shared_ptr<Event>> event_buffer_{EventBufferSize};
  size_t cur_buffer_index_ = 0;
  size_t cur_size_ = 0;

  inline bool StillBuffered() const {
    return cur_size_ > 0 and cur_buffer_index_ < cur_size_;
  }

  concurrencpp::lazy_result<bool>
  FillBuffer() {
    co_await concurrencpp::resume_on(background_executor_);

    cur_buffer_index_ = 0;
    cur_size_ = 0;

    size_t index = 0;
    while (index < EventBufferSize) {
      std::pair<bool, LineHandler> bh_p = line_handler_buffer_.NextHandler();
      if (not bh_p.first) {
        break;
      }
      LineHandler &handler =  bh_p.second;

      std::shared_ptr<Event> event_ptr = co_await log_parser_.ParseEvent(handler);
      if (not event_ptr) {
        continue;
      }

      event_buffer_[index] = event_ptr;
      ++index;
      ++cur_size_;
    }

    co_await concurrencpp::resume_on(executor_);

    co_return StillBuffered();
  }
#endif

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

#if 0
  concurrencpp::lazy_result<bool>
  GetNextEvent(std::shared_ptr<Event> &target) {
    if (not StillBuffered()) {
      if (not co_await FillBuffer()) {
        co_return false;
      }
    }

    assert(cur_size_ > 0 and cur_buffer_index_ < cur_size_);
    target = event_buffer_[cur_buffer_index_];
    ++cur_buffer_index_;
    co_return true;
  }
#endif

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
          std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override {
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

#if 0
  concurrencpp::result<void>
  produce(std::shared_ptr<concurrencpp::executor> resume_executor,
          std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override {
    throw_if_empty(resume_executor, "BufferedEventProvider::process: resume executor is null");
    throw_if_empty(tar_chan, "BufferedEventProvider::process: target channel is null");

    line_handler_buffer_.OpenFile(log_file_path_);

    std::shared_ptr<Event> event_ptr = nullptr;
    while (co_await GetNextEvent(event_ptr)) {
      assert(event_ptr and "BufferedEventProvider::process: event pointer is null");
      const bool could_push = co_await tar_chan->Push(resume_executor, event_ptr);
      throw_on(not could_push,
               "BufferedEventProvider::process: unable to push next event to target channel");
    }

    std::cout << "BufferedEventProvider exits" << std::endl;
    co_return;
  }
#endif
};

#if 0
class LogParser : public producer<std::shared_ptr<Event>>
  {
  protected:
    const std::string name_;
    const uint64_t identifier_;
    const std::string log_file_path_;
    LineReader &line_reader_;

    bool ParseTimestamp (uint64_t &timestamp);

    bool ParseAddress (uint64_t &address);

  public:
    explicit LogParser (const std::string name,
                        const std::string log_file_path,
                        LineReader &line_reader)
            : producer<std::shared_ptr<Event>> (),
              name_ (name),
              identifier_ (TraceEnvironment::GetNextParserId()),
              log_file_path_ (log_file_path),
              line_reader_ (line_reader)
    {};

    inline uint64_t GetIdent () const
    {
      return identifier_;
    }

    inline std::string GetName () const
    {
      return name_;
    }

    virtual concurrencpp::result<void>
    produce (std::shared_ptr<concurrencpp::executor> resume_executor,
             std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override
    {
      co_return;
    }
  };

class Gem5Parser : public LogParser
  {
    ComponentFilter &component_table_;

  protected:
    std::shared_ptr<Event> ParseGlobalEvent (uint64_t timestamp);

    std::shared_ptr<Event> ParseSystemSwitchCpus (uint64_t timestamp);

    std::shared_ptr<Event> ParseSystemPcPciHost (uint64_t timestamp);

    std::shared_ptr<Event>
    ParseSystemPcPciHostInterface (uint64_t timestamp);

    std::shared_ptr<Event> ParseSystemPcSimbricks (uint64_t timestamp);

    std::shared_ptr<Event> ParseSimbricksEvent (uint64_t timestamp);

  public:

    explicit Gem5Parser (const std::string name,
                         const std::string log_file_path,
                         ComponentFilter &component_table,
                         LineReader &line_reader)
            : LogParser (name, log_file_path,
                         line_reader),
              component_table_ (component_table)
    {
    }

    concurrencpp::result<void>
    produce (std::shared_ptr<concurrencpp::executor> resume_executor,
             std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override;
  };

class NicBmParser : public LogParser
  {
  protected:
    bool ParseOffLenValComma (uint64_t &off, size_t &len, uint64_t &val);

    bool ParseOpAddrLenPending (uint64_t &op, uint64_t &addr, size_t &len,
                                size_t &pending, bool with_pending);

    bool ParseMacAddress (uint64_t &address);

    bool ParseSyncInfo (bool &sync_pcie, bool &sync_eth);

  public:

    explicit NicBmParser (const std::string name,
                          const std::string log_file_path,
                          LineReader &line_reader)
            : LogParser (name, log_file_path,
                         line_reader)
    {
    }

    concurrencpp::result<void>
    produce (std::shared_ptr<concurrencpp::executor> resume_executor,
             std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override;
  };
#endif

#endif  // SIMBRICKS_TRACE_PARSER_H_