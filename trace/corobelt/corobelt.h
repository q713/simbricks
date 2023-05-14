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
#include <memory>
#include "concurrencpp/concurrencpp.h"
#include "exception.h"

template<typename ValueType, size_t Capacity = 30>
class Channel
  {
    static_assert (Capacity > 0,
                   "the channel must have a capacity of at least 1");

  private:
    concurrencpp::async_lock channel_lock_;
    concurrencpp::async_condition_variable channel_cv_;

    ValueType buffer_[Capacity];
    size_t size_ = 0;
    size_t read_index_ = 0;
    size_t write_index_ = 0;

    bool closed_ = false;
    bool poisened_ = false;

  public:
    Channel () = default;

    Channel (const Channel<ValueType, Capacity> &) = delete;

    Channel (Channel<ValueType, Capacity> &&) = delete;

    Channel<ValueType, Capacity> &operator= (
            const Channel<ValueType, Capacity> &) noexcept = delete;

    Channel<ValueType, Capacity> &operator= (
            Channel<ValueType, Capacity> &&) noexcept = delete;

    concurrencpp::result<void> close_channel (
            std::shared_ptr<concurrencpp::executor> resume_executor)
    {
      throw_if_empty<concurrencpp::executor> (resume_executor,
                                              resume_executor_null);
      {
        auto guard = co_await channel_lock_.lock (resume_executor);
        closed_ = true;
      }

      channel_cv_.notify_all ();
    }

    concurrencpp::result<void> poisen_channel (
            std::shared_ptr<concurrencpp::executor> resume_executor)
    {
      throw_if_empty<concurrencpp::executor> (resume_executor,
                                              resume_executor_null);
      {
        auto guard = co_await channel_lock_.lock (resume_executor);
        poisened_ = true;
      }

      channel_cv_.notify_all ();
    }

    // returns false if channel is closed or poisened
    concurrencpp::lazy_result<bool> push (
            std::shared_ptr<concurrencpp::executor> resume_executor,
            ValueType value)
    {
      {
        throw_if_empty<concurrencpp::executor> (resume_executor,
                                                resume_executor_null);

        auto guard = co_await channel_lock_.lock (resume_executor);
        co_await channel_cv_.await (resume_executor, guard, [this] {
          return closed_ || poisened_ || size_ < Capacity;
        });
        if (closed_ or poisened_)
        {
          channel_cv_.notify_all ();
          co_return false;
        }

        assert(not closed_ and "channel should not be closed here");
        assert(not poisened_ and "channel should not be poisened here");
        assert(size_ < Capacity and "the channel should not be full here");

        buffer_[write_index_] = std::move (value);
        write_index_ = (write_index_ + 1) % Capacity;
        ++size_;
      }

      channel_cv_.notify_one ();
      co_return true;
    }

    // returns empty optional in case channel is poisened or empty
    concurrencpp::lazy_result<std::optional<ValueType>> pop (
            std::shared_ptr<concurrencpp::executor> resume_executor)
    {
      throw_if_empty<concurrencpp::executor> (resume_executor,
                                              resume_executor_null);

      concurrencpp::scoped_async_lock guard =
              co_await channel_lock_.lock (resume_executor);

      co_await channel_cv_.await (resume_executor, guard,
                                  [this] { return closed_ || size_ > 0; });
      if (poisened_)
      {
        channel_cv_.notify_all ();
        co_return std::nullopt;
      }

      assert(not poisened_ and "channel should not be poisened here");

      if (size_ > 0)
      {
        auto result = std::move (buffer_[read_index_]);
        read_index_ = (read_index_ + 1) % Capacity;
        --size_;

        guard.unlock ();
        channel_cv_.notify_one ();

        co_return result;
      }

      co_return std::nullopt;
    }
  };

template<typename ValueType>
struct producer
  {
    explicit producer () = default;

    virtual concurrencpp::result<void> produce (
            std::shared_ptr<concurrencpp::executor> resume_executor,
            std::shared_ptr<Channel<ValueType>> &tar_chan)
    {
      co_return;
    };
  };

template<typename ValueType>
struct consumer
  {
    explicit consumer () = default;

    virtual concurrencpp::result<void> consume (
            std::shared_ptr<concurrencpp::executor> resume_executor,
            std::shared_ptr<Channel<ValueType>> &src_chan)
    {
      co_return;
    };
  };

template<typename ValueType>
struct cpipe
  {
    explicit cpipe () = default;

    virtual concurrencpp::result<void> process (
            std::shared_ptr<concurrencpp::executor> resume_executor,
            std::shared_ptr<Channel<ValueType>> &src_chan,
            std::shared_ptr<Channel<ValueType>> &tar_chan)
    {
      co_return;
    }
  };

inline void await_results (std::vector<concurrencpp::result<void>> &results)
{
  for (auto &r: results)
  {
    r.get ();
  }
}

template<typename ValueType>
inline void run_pipeline (std::shared_ptr<concurrencpp::executor> resume_executor,
                   std::shared_ptr<producer<ValueType>> prod,
                   std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes,
                   std::shared_ptr<consumer<ValueType>> cons)
{
  throw_if_empty (resume_executor, resume_executor_null);

  const size_t amount_channels = pipes.size () + 1;
  std::vector<std::shared_ptr<Channel<ValueType>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};
  channels[0] = std::make_shared<Channel<ValueType>> ();
  throw_if_empty (channels[0], channel_is_null);
  throw_if_empty (prod, producer_is_null);
  tasks[0] = prod->produce (resume_executor, channels[0]);

  for (size_t index = 0; index < pipes.size (); index++)
  {
    auto &pi = pipes[index];
    throw_if_empty (pi, pipe_is_null);

    channels[index + 1] = std::make_shared<Channel<ValueType>> ();
    throw_if_empty (channels[index + 1], channel_is_null);

    tasks[index + 1] = pi->process (resume_executor, channels[index],
                                    channels[index + 1]);
  }
  throw_if_empty (cons, consumer_is_null);
  tasks[amount_channels] = cons->consume (resume_executor,
                                       channels[amount_channels - 1]);

  for (size_t index = 0; index < amount_channels; index++)
  {
    tasks[index].get ();
    channels[index]->close_channel (resume_executor).get ();
  }
  tasks[amount_channels].get ();
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> resume_executor,
                         std::shared_ptr<producer<ValueType>> prod,
                         std::shared_ptr<consumer<ValueType>> cons) {
  std::vector<std::shared_ptr<cpipe<ValueType>>> dummy;
  run_pipeline<ValueType>(resume_executor, prod, dummy, cons);
}

#endif //SIMBRICKS_TRACE_COROBELT_H_
