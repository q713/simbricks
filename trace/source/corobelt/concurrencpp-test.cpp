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
#include <functional>

#include "sync/corobelt.h"
#include "util/exception.h"
#include "analytics/timer.h"

concurrencpp::result<void> producer_loop(
    std::shared_ptr<concurrencpp::thread_pool_executor> tpe, Channel<int> &chan,
    int range_start, int range_end) {
  for (; range_start < range_end; ++range_start) {
    bool could_write = co_await chan.Push(tpe, range_start);
    if (not could_write) {
      co_return;
    }
  }
}

concurrencpp::result<void> adder_loop(
    std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
    Channel<int> &chan_src, Channel<int> &chan_tar) {
  while (true) {
    auto res_opt = co_await chan_src.Pop(tpe);
    if (not res_opt) {
      co_return;
    }

    auto val = res_opt.value();
    val += 1;
    if (not co_await chan_tar.Push(tpe, val)) {
      co_return;
    }
  }
}

concurrencpp::result<void> consumer_loop(
    std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
    Channel<int> &chan) {
  while (true) {
    auto res_opt = co_await chan.Pop(tpe);
    if (not res_opt) {
      co_return;
    }
    std::cout << res_opt.value() << std::endl;
  }
}

struct int_prod : public producer<int> {
  int start = 0;
  int end = 0;

  explicit int_prod(int start) : producer<int>(), start(start), end(10) {
  }

  explicit int_prod(int start, int end) : producer<int>(), start(start), end(end) {
  }

  concurrencpp::result<void> produce(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);

    for (int i = start; i < end + start; i++) {
      bool could_write = co_await tar_chan->Push(resume_executor, i);
      if (not could_write) {
        break;
      }
    }

    co_await tar_chan->CloseChannel(resume_executor);
    co_return;
  };
};

struct int_cons : public consumer<int> {
  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &src_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    auto int_opt = co_await src_chan->Pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      int_opt = co_await src_chan->Pop(resume_executor);
      std::cout << "consumed: " << val << std::endl;
    }

    co_return;
  };
};

struct int_chan_cons : public consumer<int> {

  std::shared_ptr<Channel<int>> to_collector_;

  explicit int_chan_cons(std::shared_ptr<Channel<int>> to_collector)
      : consumer<int>(), to_collector_(to_collector) {
    throw_if_empty(to_collector, "to_collector is empty");
  }

  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &src_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    std::optional<int> int_opt;
    for (int_opt = co_await src_chan->Pop(resume_executor); int_opt.has_value();
         int_opt = co_await src_chan->Pop(resume_executor)) {
      int val = int_opt.value();
      auto c = to_collector_;
      co_await c->Push(resume_executor, val);
    }

    co_await to_collector_->CloseChannel(resume_executor);
    co_return;
  };
};

struct int_adder : public cpipe<int> {
  int to_add_;

  explicit int_adder() : to_add_(1) {}

  explicit int_adder(int to_add) : to_add_(to_add) {}

  concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &src_chan,
      std::shared_ptr<Channel<int>> &tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    auto int_opt = co_await src_chan->Pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      val += to_add_;
      bool could_write = co_await tar_chan->Push(resume_executor, val);
      if (not could_write) {
        break;
      }
      int_opt = co_await src_chan->Pop(resume_executor);
    }

    co_await tar_chan->CloseChannel(resume_executor);
    co_return;
  }
};

struct int_transformer : public cpipe<int> {
  const std::function<int(int)> transformer_;

  explicit int_transformer(const std::function<int(int)> transformer)
      : transformer_(transformer) {}

  concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &src_chan,
      std::shared_ptr<Channel<int>> &tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    std::optional<int> int_opt;
    for (int_opt = co_await src_chan->Pop(resume_executor); int_opt.has_value();
         int_opt = co_await src_chan->Pop(resume_executor)) {
      auto val = int_opt.value();
      val = transformer_(val);
      if (not co_await tar_chan->Push(resume_executor, val)) {
        break;
      }
    }
    co_await tar_chan->CloseChannel(resume_executor);
    co_return;
  }
};

concurrencpp::result<void>
execute_ab_stream(std::shared_ptr<concurrencpp::executor> resume_excutor) {
  auto make_even = [](int val) {
    return 2 * val;
  };
  auto make_odd = [](int val) {
    return 2 * val + 1;
  };

  auto chan_a = std::make_shared<BoundedChannel<int, 1>>();
  auto chan_b = std::make_shared<BoundedChannel<int, 1>>();

  auto p_a = std::make_shared<int_prod>(0, 10);
  auto t_a = std::make_shared<int_transformer>(make_even);
  std::vector<std::shared_ptr<cpipe<int>>> pipe_a{t_a};
  auto c_a = std::make_shared<int_chan_cons>(chan_a);
  pipeline<int> A{p_a, pipe_a, c_a};

  auto p_b = std::make_shared<int_prod>(0, 10);
  auto t_b = std::make_shared<int_transformer>(make_odd);
  std::vector<std::shared_ptr<cpipe<int>>> pipe_b{t_b};
  auto c_b = std::make_shared<int_chan_cons>(chan_b);
  pipeline<int> B{p_b, pipe_b, c_b};

  auto pa_t = run_pipeline_impl(resume_excutor, A);
  auto pb_t = run_pipeline_impl(resume_excutor, B);

  std::optional<int> int_opt;
  for (int index = 0; index < 20; index++) {
    if (index % 2 == 0) {
      int_opt = co_await chan_a->Pop(resume_excutor);
    } else {
      int_opt = co_await chan_b->Pop(resume_excutor);
    }

    if (int_opt.has_value()) {
      std::cout << "consumed: " << int_opt.value() << std::endl;
    }
  }

  co_await pa_t;
  co_await pb_t;

  co_return;
}

concurrencpp::result<void>
prod_a(std::shared_ptr<concurrencpp::executor> resume_executor, std::shared_ptr<Timer> &timer) {
  for (int i = 100; i < 1000; i += 100) {
    co_await timer->MoveForward(resume_executor, i);
    std::cout << "prod_a: " << i << std::endl;
  }
  co_await timer->Done(resume_executor);
  co_return;
}

concurrencpp::result<void>
prod_b(std::shared_ptr<concurrencpp::executor> resume_executor, std::shared_ptr<Timer> &timer) {
  for (int i = 150; i < 1050; i += 100) {
    co_await timer->MoveForward(resume_executor, i);
    std::cout << "prod_b: " << i << std::endl;
  }
  co_await timer->Done(resume_executor);
  co_return;
}

concurrencpp::result<void>
prod_a_test(std::shared_ptr<concurrencpp::executor> resume_executor, std::shared_ptr<Timer> &timer) {
  for (int i = 100; i < 1000; i += 100) {
    co_await timer->MoveForward(resume_executor, i);
    std::cout << "prod_a_test: " << i << std::endl;
  }
  co_await timer->Done(resume_executor);
  co_return;
}

concurrencpp::result<void>
prod_b_test(std::shared_ptr<concurrencpp::executor> resume_executor, std::shared_ptr<Timer> &timer) {
  for (int i = 150; i < 1050; i += 100) {
    co_await timer->MoveForward(resume_executor, i);
    std::cout << "prod_b_test: " << i << std::endl;
  }
  co_await timer->Done(resume_executor);
  co_return;
}

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
        adder_loop(thread_pool_executor, chan_src, chan_tar);
  }
  await_results(producers);
  chan_src.CloseChannel(thread_pool_executor).get();
  await_results(adders);
  chan_tar.CloseChannel(thread_pool_executor).get();
  await_results(consumers);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  auto p = std::make_shared<int_prod>(0);
  const size_t amount_adder = 30;
  std::vector<std::shared_ptr<cpipe<int>>> pipes{30};
  for (size_t i = 0; i < amount_adder; i++) {
    pipes[i] = std::make_shared<int_adder>();
  }
  auto c = std::make_shared<int_cons>();

  run_pipeline<int>(thread_pool_executor, p, c);

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  run_pipeline<int>(thread_pool_executor, p, pipes, c);

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

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  execute_ab_stream(thread_pool_executor).get();

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  auto timer = std::make_shared<Timer>(3);
  auto task_a = thread_pool_executor->submit(prod_a, thread_pool_executor, timer);
  auto task_b = thread_pool_executor->submit(prod_b, thread_pool_executor, timer);
  auto task_c = thread_pool_executor->submit(prod_b, thread_pool_executor, timer);
  task_a.get().get();
  task_b.get().get();
  task_c.get().get();

  std::cout << "###############################" << std::endl;
  std::cout << "############ BREAK ############" << std::endl;
  std::cout << "###############################" << std::endl;

  auto testTimer = std::make_shared<Timer>(5);
  auto task_a_test = thread_pool_executor->submit(prod_a_test, thread_pool_executor, testTimer);
  auto task_a2_test = thread_pool_executor->submit(prod_a_test, thread_pool_executor, testTimer);
  auto task_a3_test = thread_pool_executor->submit(prod_a_test, thread_pool_executor, testTimer);
  auto task_b_test = thread_pool_executor->submit(prod_b_test, thread_pool_executor, testTimer);
  auto task_c_test = thread_pool_executor->submit(prod_b_test, thread_pool_executor, testTimer);
  task_a_test.get().get();
  task_a2_test.get().get();
  task_a3_test.get().get();
  task_b_test.get().get();
  task_c_test.get().get();

  return 0;
}