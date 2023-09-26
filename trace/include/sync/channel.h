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

#include <concurrencpp/executors/executor.h>
#include <concurrencpp/forward_declarations.h>
#include <concurrencpp/threads/async_lock.h>
#include <concurrencpp/concurrencpp.h>

#include <list>
#include <memory>
#include <optional>
#include <functional>

#include "util/exception.h"
#include "util/utils.h"
#include "util/concepts.h"

#ifndef SIMBRICKS_TRACE_INCLUDE_SYNC_CHANNEL_H_
#define SIMBRICKS_TRACE_INCLUDE_SYNC_CHANNEL_H_

template<typename ValueType>
class NonCoroChannel {
 protected:
  std::mutex chan_numtex_;
  std::condition_variable chan_cond_var_;

  bool closed_ = false;
  bool poisened_ = false;
  size_t size_ = 0;

 public:
  explicit NonCoroChannel() = default;

  NonCoroChannel(const NonCoroChannel<ValueType> &) = delete;

  NonCoroChannel(NonCoroChannel<ValueType>
                 &&) = delete;

  virtual NonCoroChannel<ValueType> &operator=(const NonCoroChannel<ValueType> &) noexcept = delete;

  virtual NonCoroChannel<ValueType> &operator=(NonCoroChannel<ValueType> &&) noexcept = delete;

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

  virtual bool Push(ValueType value) {
    return false;
  };

  virtual bool TryPush(ValueType value) {
    return false;
  }

  virtual std::optional<ValueType> Pop() {
    return std::nullopt;
  }

  virtual std::optional<ValueType> TryPop() {
    return std::nullopt;
  }

  virtual std::optional<ValueType> TryPopOnTrue(std::function<bool(ValueType &)> &predicate) {
    return std::nullopt;
  }
};

template<typename ValueType, size_t BufferSize> requires SizeLagerZero<BufferSize>
class NonCoroBufferedChannel : public NonCoroChannel<ValueType> {

  std::vector<ValueType> buffer_{BufferSize};
  size_t read_index_ = 0;
  size_t write_index_ = 0;

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

  // returns false if channel is closed or poisened
  bool Push(ValueType value) {
    {
      std::unique_lock lock{this->chan_numtex_};
      this->chan_cond_var_.wait(lock, [this] {
        return this->closed_ or this->poisened_ or this->size_ < BufferSize;
      });
      if (this->closed_ or this->poisened_) {
        this->chan_cond_var_.notify_all();
        return false;
      }
      assert(not this->closed_ and "channel should not be closed here");
      assert(not this->poisened_ and "channel should not be poisened here");

      perform_write(std::move(value));
    }

    this->chan_cond_var_.notify_all();
    return true;
  }

  // returns false if channel is closed or poisened
  bool TryPush(ValueType value) {
    {
      std::unique_lock<std::mutex> lock{this->chan_numtex_};
      if (this->closed_ or this->poisened_ or this->size_ >= BufferSize) {
        return false;
      }
      assert(not this->closed_ and "channel should not be closed here");
      assert(not this->poisened_ and "channel should not be poisened here");
      perform_write(std::move(value));
    }
    this->chan_cond_var_.notify_all();
    return true;
  }

  // returns empty optional in case channel is poisened or empty
  std::optional<ValueType> Pop() {
    std::unique_lock lock{this->chan_numtex_};
    this->chan_cond_var_.wait(lock, [this] {
      return this->poisened_ || this->closed_ || this->size_ > 0;
    });
    if (this->poisened_) {
      this->chan_cond_var_.notify_all();
      return std::nullopt;
    }
    assert(not this->poisened_ and "channel should not be poisened here");
    if (this->size_ == 0) {
      lock.unlock();
      this->chan_cond_var_.notify_all();
      return std::nullopt;
    }
    assert(this->size_ > 0 and "trying to read from empty channel");
    auto result = perform_read();
    lock.unlock();
    this->chan_cond_var_.notify_all();
    return result;
  }

  std::optional<ValueType> TryPop() {
    std::unique_lock lock{this->chan_numtex_};
    if (this->poisened_ or this->size_ == 0) {
      lock.unlock();
      this->chan_cond_var_.notify_all();
      return std::nullopt;
    }
    auto result = perform_read();
    lock.unlock();
    this->chan_cond_var_.notify_all();
    return std::move(result);
  }

  std::optional<ValueType> TryPopOnTrue(std::function<bool(ValueType &)> &predicate) {
    std::unique_lock lock{this->chan_numtex_};
    if (this->poisened_ or this->size_ == 0) {
      lock.unlock();
      this->chan_cond_var_.notify_all();
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
    this->chan_cond_var_.notify_all();
    return std::move(result);
  }
};

template<typename ValueType>
class CoroChannel {
 protected:
  concurrencpp::async_lock channel_lock_;
  concurrencpp::async_condition_variable channel_cv_;

  size_t size_ = 0;
  bool closed_ = false;
  bool poisened_ = false;

 public:
  explicit CoroChannel() = default;

  CoroChannel(const CoroChannel<ValueType> &) = delete;

  CoroChannel(CoroChannel<ValueType> &&) = delete;

  CoroChannel<ValueType> &operator=(const CoroChannel<ValueType> &) noexcept = delete;

  CoroChannel<ValueType> &operator=(CoroChannel<ValueType> &&) noexcept = delete;

  concurrencpp::lazy_result<bool> Empty(
      std::shared_ptr<concurrencpp::executor> resume_executor) {
    concurrencpp::scoped_async_lock guard =
        co_await channel_lock_.lock(resume_executor);
    co_return size_ == 0;
  }

  concurrencpp::lazy_result<size_t> GetSize(
      std::shared_ptr<concurrencpp::executor> resume_executor) {
    concurrencpp::scoped_async_lock guard =
        co_await channel_lock_.lock(resume_executor);
    co_return size_;
  }

  virtual concurrencpp::result<void> Display(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::ostream &out,
      std::function<std::ostream &(std::ostream &, ValueType &)>
      &value_printer) {
    co_return;
  }

  concurrencpp::result<void> CloseChannel(
      std::shared_ptr<concurrencpp::executor> resume_executor) {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    {
      concurrencpp::scoped_async_lock guard =
          co_await channel_lock_.lock(resume_executor);
      closed_ = true;
    }

    channel_cv_.notify_all();
  }

  concurrencpp::result<void> PoisenChannel(
      std::shared_ptr<concurrencpp::executor> resume_executor) {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    {
      concurrencpp::scoped_async_lock guard =
          co_await channel_lock_.lock(resume_executor);
      poisened_ = true;
    }

    channel_cv_.notify_all();
  }

  virtual concurrencpp::lazy_result<bool> Push(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) {
    co_return false;
  };

  virtual concurrencpp::lazy_result<bool> TryPush(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) {
    co_return false;
  }

  virtual concurrencpp::lazy_result<std::optional<ValueType>> Pop(
      std::shared_ptr<concurrencpp::executor> resume_executor) {
    co_return std::nullopt;
  }

  virtual concurrencpp::lazy_result<std::optional<ValueType>> TryPop(
      std::shared_ptr<concurrencpp::executor> resume_executor) {
    co_return std::nullopt;
  }

  virtual concurrencpp::lazy_result<std::optional<ValueType>> TryPopOnTrue(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::function<bool(ValueType &)> &predicate) {
    co_return std::nullopt;
  }
};

template<typename ValueType, size_t Capacity = 30> requires SizeLagerZero<Capacity>
class CoroBoundedChannel : public CoroChannel<ValueType> {

 private:
  std::vector<ValueType> buffer_{Capacity};
  size_t read_index_ = 0;
  size_t write_index_ = 0;

  // NOTE: the lock must be held when calling this method
  void perform_write(ValueType value) {
    assert(this->size_ < Capacity and "the channel should not be full here");
    assert(write_index_ < Capacity and "cannot write out of bound");
    assert(write_index_ >= 0 and "cannot write out of bound");
    buffer_[write_index_] = std::move(value);
    write_index_ = (write_index_ + 1) % Capacity;
    ++(this->size_);
  }

  // NOTE: the lock must be held when calling this method
  ValueType perform_read() {
    assert(this->size_ > 0 and "the channel should not be empty here");
    assert(read_index_ < Capacity and "cannot read out of bound");
    assert(read_index_ >= 0 and "cannot read out of bound");
    auto result = std::move(buffer_[read_index_]);
    read_index_ = (read_index_ + 1) % Capacity;
    --(this->size_);
    return std::move(result);
  }

 public:
  CoroBoundedChannel() : CoroChannel<ValueType>() {};

  CoroBoundedChannel(const CoroBoundedChannel<ValueType, Capacity> &) = delete;

  CoroBoundedChannel(CoroBoundedChannel<ValueType, Capacity> &&) = delete;

  CoroBoundedChannel<ValueType, Capacity> &operator=(
      const CoroBoundedChannel<ValueType, Capacity> &) noexcept = delete;

  CoroBoundedChannel<ValueType, Capacity> &operator=(
      CoroBoundedChannel<ValueType, Capacity> &&) noexcept = delete;

  concurrencpp::result<void> Display(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::ostream &out,
      std::function<std::ostream &(std::ostream &, ValueType &)> &value_printer)
  override {
    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    out << "Channel:" << '\n';
    out << "capacity=" << Capacity << '\n';
    out << "size=" << this->size_ << '\n';
    out << "read_index=" << read_index_ << '\n';
    out << "write_index=" << write_index_ << '\n';
    out << "closed=" << this->closed_ << '\n';
    out << "poisened=" << this->poisened_ << '\n';
    out << "Buffer={" << '\n';
    size_t index = 0;
    while (this->size_ > 0 and index < this->size_) {
      auto &val = buffer_[read_index_ + index % Capacity];
      ++index;
      value_printer(out, val) << '\n';
    }
    out << "}" << '\n';
  }

  // returns false if channel is closed or poisened
  concurrencpp::lazy_result<bool> Push(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    {
      concurrencpp::scoped_async_lock guard =
          co_await this->channel_lock_.lock(resume_executor);
      co_await this->channel_cv_.await(resume_executor, guard, [this] {
        return this->closed_ or this->poisened_ or this->size_ < Capacity;
      });
      if (this->closed_ or this->poisened_) {
        guard.unlock();
        this->channel_cv_.notify_all();
        co_return false;
      }
      assert(not this->closed_ and "channel should not be closed here");
      assert(not this->poisened_ and "channel should not be poisened here");

      perform_write(std::move(value));
    }

    this->channel_cv_.notify_all();
    co_return true;
  }

  // returns false if channel is closed or poisened
  concurrencpp::lazy_result<bool> TryPush(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    {
      concurrencpp::scoped_async_lock guard =
          co_await this->channel_lock_.lock(resume_executor);
      if (this->closed_ or this->poisened_ or this->size_ >= Capacity) {
        co_return false;
      }

      assert(not this->closed_ and "channel should not be closed here");
      assert(not this->poisened_ and "channel should not be poisened here");

      perform_write(std::move(value));
    }

    this->channel_cv_.notify_all();
    co_return true;
  }

  // returns empty optional in case channel is poisened or empty
  concurrencpp::lazy_result<std::optional<ValueType>> Pop(
      std::shared_ptr<concurrencpp::executor> resume_executor) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());

    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    co_await this->channel_cv_.await(resume_executor, guard, [this] {
      return this->poisened_ || this->closed_ || this->size_ > 0;
    });
    if (this->poisened_) {
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }
    assert(not this->poisened_ and "channel should not be poisened here");

    if (this->size_ == 0) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }
    assert(this->size_ > 0 and "trying to read from empty channel");

    auto result = perform_read();

    guard.unlock();
    this->channel_cv_.notify_all();

    co_return result;
  }

  concurrencpp::lazy_result<std::optional<ValueType>> TryPop(
      std::shared_ptr<concurrencpp::executor> resume_executor) override {
    throw_if_empty(resume_executor, TraceException::kResumeExecutorNull, source_loc::current());

    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    if (this->poisened_) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }

    if (this->size_ == 0) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }

    auto result = perform_read();

    guard.unlock();
    this->channel_cv_.notify_all();

    co_return std::move(result);
  }

  concurrencpp::lazy_result<std::optional<ValueType>> TryPopOnTrue(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::function<bool(ValueType &)> &predicate) override {
    throw_if_empty(resume_executor, TraceException::kResumeExecutorNull, source_loc::current());

    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    if (this->poisened_ or this->size_ == 0) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }

    assert(read_index_ < Capacity and "cannot read out of bound");
    assert(read_index_ >= 0 and "cannot read out of bound");
    ValueType &value = buffer_[read_index_];
    if (not predicate(value)) {
      co_return std::nullopt;
    }

    auto result = perform_read();

    guard.unlock();
    this->channel_cv_.notify_all();

    co_return std::move(result);
  }
};

template<typename ValueType>
class CoroUnBoundedChannel : public CoroChannel<ValueType> {
  std::list<ValueType> buffer_;

 public:
  CoroUnBoundedChannel() : CoroChannel<ValueType>() {};

  CoroUnBoundedChannel(const CoroChannel<ValueType> &) = delete;

  CoroUnBoundedChannel(CoroChannel<ValueType> &&) = delete;

  CoroUnBoundedChannel<ValueType> &operator=(
      const CoroUnBoundedChannel<ValueType> &) noexcept = delete;

  CoroUnBoundedChannel<ValueType> &operator=(
      CoroUnBoundedChannel<ValueType> &&) noexcept = delete;

  concurrencpp::result<void> Display(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::ostream &out,
      std::function<std::ostream &(std::ostream &, ValueType &)> &value_printer)
  override {
    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    out << "Channel:" << '\n';
    out << "size=" << this->size_ << '\n';
    out << "closed=" << this->closed_ << '\n';
    out << "poisened=" << this->poisened_ << '\n';
    out << "Buffer={" << '\n';
    for (auto &val : buffer_) {
      value_printer(out, val);
    }
    out << "}" << '\n';
  }

  // returns false if channel is closed or poisened
  concurrencpp::lazy_result<bool> Push(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    {
      concurrencpp::scoped_async_lock guard =
          co_await this->channel_lock_.lock(resume_executor);
      if (this->closed_ or this->poisened_) {
        this->channel_cv_.notify_all();
        co_return false;
      }
      assert(not this->closed_ and "channel should not be closed here");
      assert(not this->poisened_ and "channel should not be poisened here");

      buffer_.push_back(std::move(value));
      ++(this->size_);
    }

    this->channel_cv_.notify_all();
    co_return true;
  }

  // returns false if channel is closed or poisened
  concurrencpp::lazy_result<bool> TryPush(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    this->channel_cv_.notify_all();
    co_return co_await Push(resume_executor, std::move(value));
  }

  // returns empty optional in case channel is poisened or empty
  concurrencpp::lazy_result<std::optional<ValueType>> Pop(
      std::shared_ptr<concurrencpp::executor> resume_executor) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());

    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    co_await this->channel_cv_.await(resume_executor, guard, [this] {
      return this->poisened_ || this->closed_ || this->size_ > 0;
    });
    if (this->poisened_) {
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }
    assert(not this->poisened_ and "channel should not be poisened here");

    if (this->size_ == 0) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }
    assert(this->size_ > 0 and "trying to read from empty channel");

    auto result = buffer_.front();
    buffer_.pop_front();
    --(this->size_);

    guard.unlock();
    this->channel_cv_.notify_all();

    co_return result;
  }

  concurrencpp::lazy_result<std::optional<ValueType>> TryPop(
      std::shared_ptr<concurrencpp::executor> resume_executor) override {
    throw_if_empty(resume_executor, TraceException::kResumeExecutorNull, source_loc::current());

    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    if (this->poisened_ or this->size_ == 0) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }

    auto result = buffer_.front();
    buffer_.pop_front();
    --(this->size_);

    guard.unlock();
    this->channel_cv_.notify_all();

    co_return std::move(result);
  }

  concurrencpp::lazy_result<std::optional<ValueType>> TryPopOnTrue(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::function<bool(ValueType &)> &predicate) override {
    throw_if_empty(resume_executor, TraceException::kResumeExecutorNull, source_loc::current());

    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    if (this->poisened_ or this->size_ == 0) {
      guard.unlock();
      this->channel_cv_.notify_all();
      co_return std::nullopt;
    }

    auto result = buffer_.front();
    if (not predicate(result)) {
      co_return std::nullopt;
    }
    buffer_.pop_front();
    --(this->size_);

    guard.unlock();
    this->channel_cv_.notify_all();

    co_return std::move(result);
  }
};

#endif //SIMBRICKS_TRACE_INCLUDE_SYNC_CHANNEL_H_
