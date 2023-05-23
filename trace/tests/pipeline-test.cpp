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

#include <catch2/catch_all.hpp>
#include <iostream>
#include <memory>
#include <queue>

#include "corobelt/corobelt.h"
#include "util/exception.h"

struct int_prod : public producer<int> {
  int start = 0;

  int_prod(int s) : producer<int>(), start(s) {
  }

  concurrencpp::result<void> produce(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);

    for (int i = start; i < 3 + start; i++) {
      bool could_write = co_await tar_chan->push(resume_executor, i);
      if (not could_write) {
        break;
      }
    }

    co_return;
  };
};

struct int_cons : public consumer<int> {

  const std::string prefix_;
  std::stringstream &ss_;

  int_cons(const std::string prefix, std::stringstream &ss) : consumer<int>(), prefix_(prefix), ss_(ss) {}

  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &src_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    auto int_opt = co_await src_chan->pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      int_opt = co_await src_chan->pop(resume_executor);
      ss_ << prefix_ << "-consumed: " << val << std::endl;
    }

    co_return;
  };
};

struct int_adder : public cpipe<int> {
  concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<int>> &src_chan,
      std::shared_ptr<Channel<int>> &tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           resume_executor_null);
    throw_if_empty<Channel<int>>(tar_chan, channel_is_null);
    throw_if_empty<Channel<int>>(src_chan, channel_is_null);

    auto int_opt = co_await src_chan->pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      val += 10;
      bool could_write = co_await tar_chan->push(resume_executor, val);
      if (not could_write) {
        break;
      }
      int_opt = co_await src_chan->pop(resume_executor);
    }

    co_return;
  }
};

TEST_CASE("Test pipeline wrapper construct", "[run_pipeline]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 3;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  auto prod_a = std::make_shared<int_prod>(0);
  auto prod_b = std::make_shared<int_prod>(100);

  size_t amount_adder = 30;
  std::vector<std::shared_ptr<cpipe<int>>> adders_a{amount_adder};
  std::vector<std::shared_ptr<cpipe<int>>> adders_b{amount_adder};
  for (size_t index = 0; index < amount_adder; index++) {
    adders_a[index] = std::make_shared<int_adder>();
    adders_b[index] = std::make_shared<int_adder>();
  }

  std::stringstream ss_a;
  std::stringstream ss_b;
  auto cons_a = std::make_shared<int_cons>("a", ss_a);
  auto cons_b = std::make_shared<int_cons>("b", ss_b);

  pipeline<int> simple_a{prod_a, adders_a, cons_a};
  pipeline<int> simple_b{prod_b, adders_b, cons_b};

  std::vector<pipeline<int>> pipelines{simple_a, simple_b};

  SECTION("simple pipeline without pipes") {
    REQUIRE_NOTHROW(run_pipeline<int>(thread_pool_executor, prod_a, cons_a));

    REQUIRE(ss_a.str() == "a-consumed: 0\na-consumed: 1\na-consumed: 2\n");
  }

  SECTION("simple pipeline with pipes") {
    REQUIRE_NOTHROW(run_pipeline<int>(thread_pool_executor, prod_a, adders_a, cons_a));

    REQUIRE(ss_a.str() == "a-consumed: 300\na-consumed: 301\na-consumed: 302\n");
  }

  SECTION("simple pipeline with wrapper") {
    REQUIRE_NOTHROW(run_pipeline<int>(thread_pool_executor, simple_a));

    REQUIRE(ss_a.str() == "a-consumed: 300\na-consumed: 301\na-consumed: 302\n");
  }

  SECTION("multiple pipelines") {
    REQUIRE_NOTHROW(run_pipelines<int>(thread_pool_executor, pipelines));

    REQUIRE(ss_a.str() == "a-consumed: 300\na-consumed: 301\na-consumed: 302\n");
    REQUIRE(ss_b.str() == "b-consumed: 400\nb-consumed: 401\nb-consumed: 402\n");
  }

  SECTION("multiple pipelines - parallel") {
    REQUIRE_NOTHROW(run_pipelines_parallel<int>(thread_pool_executor, pipelines));

    REQUIRE(ss_a.str() == "a-consumed: 300\na-consumed: 301\na-consumed: 302\n");
    REQUIRE(ss_b.str() == "b-consumed: 400\nb-consumed: 401\nb-consumed: 402\n");
  }
}


