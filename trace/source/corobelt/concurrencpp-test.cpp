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

#include <iostream>
#include <memory>
#include <queue>

#include "corobelt/corobelt.h"
#include "util/exception.h"


concurrencpp::result<void> producer_loop(
        std::shared_ptr<concurrencpp::thread_pool_executor> tpe, Channel<int>& chan,
        int range_start, int range_end) {
  for (; range_start < range_end; ++range_start) {
    bool could_write = co_await chan.push(tpe, std::move(range_start));
    if (not could_write) {
      co_return;
    }
  }
}

concurrencpp::result<void> adder_loop(
        std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
        Channel<int>& chan_src, Channel<int>& chan_tar) {
  while (true) {
    auto res_opt = co_await chan_src.pop(tpe);
    if (not res_opt) {
      co_return;
    }

    auto val = res_opt.value();
    val += 1;
    if (not co_await chan_tar.push(tpe, std::move(val))) {
      co_return;
    }
  }
}

concurrencpp::result<void> consumer_loop(
        std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
        Channel<int>& chan) {
  while (true) {
    auto res_opt = co_await chan.pop(tpe);
    if (not res_opt) {
      co_return;
    }
    std::cout << res_opt.value() << std::endl;
  }
}

struct int_prod : public producer<int> {
  int start = 0;

  int_prod(int s) : producer<int>(), start(s) {
  }

  concurrencpp::result<void> produce(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>>& tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);

    for (int i = start; i < 10 + start; i++) {
      bool could_write = co_await tar_chan->push(resume_executor, std::move(i));
      if (not could_write) {
        break;
      }
    }

    co_return;
  };
};

struct int_cons : public consumer<int> {
  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>>& src_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    auto int_opt = co_await src_chan->pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      int_opt = co_await src_chan->pop(resume_executor);
      std::cout << "consumed: " << val << std::endl;
    }

    co_return;
  };
};

struct int_adder : public cpipe<int> {
   concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>>& src_chan,
      std::shared_ptr<Channel<int>>& tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    auto int_opt = co_await src_chan->pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      val += 10;
      bool could_write = co_await tar_chan->push(resume_executor, std::move(val));
      if (not could_write) {
        break;
      }
      int_opt = co_await src_chan->pop(resume_executor);
    }

    co_return;
  }
};



int main() {
  auto options = concurrencpp::runtime_options();
  options.max_background_threads = 0;
  options.max_cpu_threads = 1;
  concurrencpp::runtime runtime{options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  Channel<int> chan_src;
  Channel<int> chan_tar;
  std::vector<concurrencpp::result<void>> producers{1};
  std::vector<concurrencpp::result<void>> adders{4};
  std::vector<concurrencpp::result<void>> consumers{4};
  for (int i = 0; i < 4; i++) {
    consumers[i] = consumer_loop(thread_pool_executor, chan_tar);
  }
  for (int i = 0; i < 1; i++) {
    producers[i] =
        producer_loop(thread_pool_executor, chan_src, i * 5, (i + 1) * 5);
  }
  for (int i = 0; i < 4; i++) {
    adders[i] =
            adder_loop (thread_pool_executor, chan_src, chan_tar);
  }
  await_results(producers);
  chan_src.close_channel(thread_pool_executor).get();
  await_results(adders);
  chan_tar.close_channel (thread_pool_executor).get ();
  await_results(consumers);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  auto p = std::make_shared<int_prod>(0);
  const size_t amount_adder = 30;
  std::vector<std::shared_ptr<cpipe<int>>> pipes{30};
  for (size_t i = 0; i< amount_adder; i++) {
    pipes[i] = std::make_shared<int_adder>();
  }
  auto c = std::make_shared<int_cons>();

  run_pipeline<int>(thread_pool_executor, p, c);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  run_pipeline<int> (thread_pool_executor, p, pipes, c);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  pipeline<int> pl_a{p, pipes, c};
  pipeline<int> pl_b{p, pipes, c};
  std::vector<pipeline<int>> pipelines{pl_a, pl_b};

  run_pipeline(thread_pool_executor, pl_a);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  run_pipelines<int>(thread_pool_executor, pipelines);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  run_pipelines_parallel(thread_pool_executor, pipelines);

  return 0;
}