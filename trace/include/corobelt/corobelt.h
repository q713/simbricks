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

#ifndef SIMBRICKS_TRACE_COROBELT_H_
#define SIMBRICKS_TRACE_COROBELT_H_

#include <concurrencpp/executors/executor.h>
#include <concurrencpp/forward_declarations.h>
#include <concurrencpp/threads/async_lock.h>

#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <optional>

#include "concurrencpp/concurrencpp.h"
#include "util/exception.h"
#include "util/factory.h"
#include "util/utils.h"

template<typename ValueType>
class Channel {
 protected:
  concurrencpp::async_lock channel_lock_;
  concurrencpp::async_condition_variable channel_cv_;

  size_t size_ = 0;
  bool closed_ = false;
  bool poisened_ = false;

 public:
  explicit Channel() = default;

  Channel(const Channel<ValueType> &) = delete;

  Channel(Channel<ValueType> &&) = delete;

  Channel<ValueType> &operator=(const Channel<ValueType> &) noexcept = delete;

  Channel<ValueType> &operator=(Channel<ValueType> &&) noexcept = delete;

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
                                           resume_executor_null);
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
                                           resume_executor_null);
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
class BoundedChannel : public Channel<ValueType> {

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
  BoundedChannel() : Channel<ValueType>() {};

  BoundedChannel(const BoundedChannel<ValueType, Capacity> &) = delete;

  BoundedChannel(BoundedChannel<ValueType, Capacity> &&) = delete;

  BoundedChannel<ValueType, Capacity> &operator=(
      const BoundedChannel<ValueType, Capacity> &) noexcept = delete;

  BoundedChannel<ValueType, Capacity> &operator=(
      BoundedChannel<ValueType, Capacity> &&) noexcept = delete;

  concurrencpp::result<void> Display(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::ostream &out,
      std::function<std::ostream &(std::ostream &, ValueType &)> &value_printer)
  override {
    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    out << "Channel:" << std::endl;
    out << "capacity=" << Capacity << std::endl;
    out << "size=" << this->size_ << std::endl;
    out << "read_index=" << read_index_ << std::endl;
    out << "write_index=" << write_index_ << std::endl;
    out << "closed=" << this->closed_ << std::endl;
    out << "poisened=" << this->poisened_ << std::endl;
    out << "Buffer={" << std::endl;
    size_t index = 0;
    while (this->size_ > 0 and index < this->size_) {
      auto &val = buffer_[read_index_ + index % Capacity];
      ++index;
      value_printer(out, val) << std::endl;
    }
    out << "}" << std::endl;
  }

  // returns false if channel is closed or poisened
  concurrencpp::lazy_result<bool> Push(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    {
      concurrencpp::scoped_async_lock guard =
          co_await this->channel_lock_.lock(resume_executor);
      co_await this->channel_cv_.await(resume_executor, guard, [this] {
        return this->closed_ or this->poisened_ or this->size_ < Capacity;
      });
      if (this->closed_ or this->poisened_) {
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
                                           resume_executor_null);
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
                                           resume_executor_null);

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
    throw_if_empty(resume_executor, resume_executor_null);

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
    throw_if_empty(resume_executor, resume_executor_null);

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
class UnBoundedChannel : public Channel<ValueType> {
  std::list<ValueType> buffer_;

 public:
  UnBoundedChannel() : Channel<ValueType>() {};

  UnBoundedChannel(const BoundedChannel<ValueType> &) = delete;

  UnBoundedChannel(BoundedChannel<ValueType> &&) = delete;

  UnBoundedChannel<ValueType> &operator=(
      const BoundedChannel<ValueType> &) noexcept = delete;

  UnBoundedChannel<ValueType> &operator=(
      BoundedChannel<ValueType> &&) noexcept = delete;

  concurrencpp::result<void> Display(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::ostream &out,
      std::function<std::ostream &(std::ostream &, ValueType &)> &value_printer)
  override {
    concurrencpp::scoped_async_lock guard =
        co_await this->channel_lock_.lock(resume_executor);

    out << "Channel:" << std::endl;
    out << "size=" << this->size_ << std::endl;
    out << "closed=" << this->closed_ << std::endl;
    out << "poisened=" << this->poisened_ << std::endl;
    out << "Buffer={" << std::endl;
    for (auto &val : buffer_) {
      value_printer(out, val);
    }
    out << "}" << std::endl;
  }

  // returns false if channel is closed or poisened
  concurrencpp::lazy_result<bool> Push(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      ValueType value) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
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
                                           resume_executor_null);
    this->channel_cv_.notify_all();
    co_return co_await Push(resume_executor, std::move(value));
  }

  // returns empty optional in case channel is poisened or empty
  concurrencpp::lazy_result<std::optional<ValueType>> Pop(
      std::shared_ptr<concurrencpp::executor> resume_executor) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);

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
    throw_if_empty(resume_executor, resume_executor_null);

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
    throw_if_empty(resume_executor, resume_executor_null);

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

template<typename ValueType>
struct producer {
  explicit producer() = default;

  virtual concurrencpp::result<void> produce(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<ValueType>> &tar_chan) {
    co_return;
  };
};

template<typename ValueType>
struct consumer {
  explicit consumer() = default;

  virtual concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<ValueType>> &src_chan) {
    co_return;
  };
};

template<typename ValueType>
struct cpipe {
  explicit cpipe() = default;

  virtual concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<ValueType>> &src_chan,
      std::shared_ptr<Channel<ValueType>> &tar_chan) {
    co_return;
  }
};

/* Wrapper class to allow passing multiple pipelines to method */
template<typename ValueType>
struct pipeline {
  std::shared_ptr<producer<ValueType>> prod_;
  std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes_;
  std::shared_ptr<consumer<ValueType>> cons_;

  explicit pipeline(std::shared_ptr<producer<ValueType>> prod,
                    std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes,
                    std::shared_ptr<consumer<ValueType>> cons)
      : prod_(prod), pipes_(pipes), cons_(cons) {
  }
};

template<typename ValueType>
inline concurrencpp::result<void> run_pipeline_impl(
    std::shared_ptr<concurrencpp::executor> executor,
    pipeline<ValueType> &pipeline) {
  throw_if_empty(executor, resume_executor_null);

  const size_t amount_channels = pipeline.pipes_.size() + 1;
  std::vector<std::shared_ptr<Channel<ValueType>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};
  //std::vector<concurrencpp::result<concurrencpp::result<void>>> tasks{amount_channels + 1};
  channels[0] = create_shared<BoundedChannel<ValueType>>(channel_is_null);
  throw_if_empty(pipeline.prod_, producer_is_null);
  //tasks[0] = executor->submit([&](std::shared_ptr<Channel<ValueType>> tar) {
  //  return pipeline.prod_->produce(executor, tar);
  //}, channels[0]);
  tasks[0] = pipeline.prod_->produce(executor, channels[0]);

  for (size_t index = 0; index < pipeline.pipes_.size(); index++) {
    auto &pipe = pipeline.pipes_[index];
    throw_if_empty(pipe, pipe_is_null);

    channels[index + 1] =
        create_shared<BoundedChannel<ValueType>>(channel_is_null);

    //tasks[index + 1] = executor->submit([&](std::shared_ptr<Channel<ValueType>> src,
    //                                        std::shared_ptr<Channel<ValueType>> tar) {
    //  return pipe->process(executor, src, tar);
    //}, channels[index], channels[index + 1]);
    tasks[index + 1] =
        pipe->process(executor, channels[index], channels[index + 1]);
  }
  throw_if_empty(pipeline.cons_, consumer_is_null);
  //tasks[amount_channels] = executor->submit([&](std::shared_ptr<Channel<ValueType>> src) {
  //  return pipeline.cons_->consume(executor, src);
  //}, channels[amount_channels - 1]);
  tasks[amount_channels] =
      pipeline.cons_->consume(executor, channels[amount_channels - 1]);

  for (size_t index = 0; index < amount_channels; index++) {
    //co_await co_await tasks[index];
    co_await tasks[index];
    co_await channels[index]->CloseChannel(executor);
  }
  co_await tasks[amount_channels];
  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> run_pipeline_impl_parallel(
    concurrencpp::executor_tag,
    std::shared_ptr<concurrencpp::executor> executor,
    pipeline<ValueType> &pipeline) {
  co_await run_pipeline_impl(executor, pipeline);
  co_return;
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> executor,
                         pipeline<ValueType> &pipeline) {
  try {
    run_pipeline_impl(executor, pipeline).get();
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> executor,
                         std::shared_ptr<producer<ValueType>> prod,
                         std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes,
                         std::shared_ptr<consumer<ValueType>> cons) {
  pipeline<ValueType> pipeline{prod, pipes, cons};

  try {
    run_pipeline_impl(executor, pipeline).get();
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> executor,
                         std::shared_ptr<producer<ValueType>> prod,
                         std::shared_ptr<consumer<ValueType>> cons) {
  std::vector<std::shared_ptr<cpipe<ValueType>>> dummy;
  run_pipeline<ValueType>(executor, prod, dummy, cons);
}

template<typename ValueType>
inline void run_pipelines(std::shared_ptr<concurrencpp::executor> executor,
                          std::vector<pipeline<ValueType>> &pipelines) {
  size_t amount_tasks = pipelines.size();
  std::vector<concurrencpp::result<void>> pipelns{amount_tasks};

  try {
    // create asynchronous(NOTE: asynchronous does not equal parallel)
    // coroutines
    for (size_t index = 0; index < amount_tasks; index++) {
      auto &pipel = pipelines[index];
      pipelns[index] = run_pipeline_impl(executor, pipel);
    }
    // suspend to get result
    for (size_t index = 0; index < amount_tasks; index++) {
      pipelns[index].get();
    }
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }
}

template<typename ValueType>
inline void run_pipelines_parallel(
    std::shared_ptr<concurrencpp::executor> executor,
    std::vector<pipeline<ValueType>> &pipelines) {
  throw_if_empty(executor, resume_executor_null);
  size_t amount_tasks = pipelines.size();
  //std::vector<concurrencpp::result<concurrencpp::result<void>>> pipelns{
  //    amount_tasks};
  std::vector<concurrencpp::result<void>> pipelns{amount_tasks};

  try {
    // create asynchronous(NOTE: asynchronous does not equal parallel) coroutines
    for (size_t index = 0; index < amount_tasks; index++) {
      //pipelns[index] = executor->submit(run_pipeline_impl<ValueType>, executor,
      //                                  pipelines[index]);
      pipelns[index] = run_pipeline_impl_parallel({}, executor, pipelines[index]);
    }
    // suspend to get result
    for (size_t index = 0; index < amount_tasks; index++) {
      pipelns[index].get();
      std::cout << "one pipeline finished" << std::endl;
    }
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }

  std::cout << "all pipelines finished" << std::endl;
}

inline void await_results(std::vector<concurrencpp::result<void>> &results) {
  for (auto &res : results) {
    res.get();
  }
}

#endif  // SIMBRICKS_TRACE_COROBELT_H_
