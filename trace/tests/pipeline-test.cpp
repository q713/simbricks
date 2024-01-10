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

#include "sync/corobelt.h"
#include "util/exception.h"

struct int_prod : public producer<int> {
  int start = 0;

  int_prod(int s) : producer<int>(), start(s) {
  }

  concurrencpp::result<void> produce(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<int>> tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    throw_if_empty<CoroChannel<int>>(tar_chan, TraceException::kChannelIsNull,
                                     source_loc::current());

    for (int i = start; i < 3 + start; i++) {
      bool could_write = co_await tar_chan->Push(resume_executor, i);
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
      std::shared_ptr<CoroChannel<int>> src_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    throw_if_empty<CoroChannel<int>>(src_chan, TraceException::kChannelIsNull,
                                     source_loc::current());

    auto int_opt = co_await src_chan->Pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      int_opt = co_await src_chan->Pop(resume_executor);
      ss_ << prefix_ << "-consumed: " << val << '\n';
    }

    co_return;
  };
};

struct int_adder : public cpipe<int> {
  concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<int>> src_chan,
      std::shared_ptr<CoroChannel<int>> tar_chan) override {
    throw_if_empty<concurrencpp::executor>(resume_executor,
                                           TraceException::kResumeExecutorNull,
                                           source_loc::current());
    throw_if_empty<CoroChannel<int>>(tar_chan, TraceException::kChannelIsNull,
                                     source_loc::current());
    throw_if_empty<CoroChannel<int>>(src_chan, TraceException::kChannelIsNull,
                                     source_loc::current());

    auto int_opt = co_await src_chan->Pop(resume_executor);

    while (int_opt.has_value()) {
      auto val = int_opt.value();
      val += 10;
      bool could_write = co_await tar_chan->Push(resume_executor, std::move(val));
      if (not could_write) {
        break;
      }
      int_opt = co_await src_chan->Pop(resume_executor);
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

  const size_t amount_adder = 30;
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

class ProducerInt : public Producer<int> {
  int start = 0;
  int end = 1;

 public:
  ProducerInt(int s, int e) : Producer<int>(), start(s), end(e) {
  }

  explicit operator bool() noexcept override {
    return start < end;
  }

  concurrencpp::result<std::optional<int>> produce(std::shared_ptr<concurrencpp::executor> executor) override {
    int res = start;
    start++;
    co_return res;
  }
};

class AdderInt : public Handler<int> {
 public:
  explicit AdderInt() : Handler<int>() {}

  concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor, int &value) override {
    value += 1;
    co_return true;
  }
};

class PrinterInt : public Consumer<int> {
  std::ostream &out_;

 public:

  explicit PrinterInt(std::ostream &out) : Consumer<int>(), out_(out) {}

  concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor, int value) override {
    out_ << "consumed: " << value << '\n';
    co_return;
  }

};

#define CREATE_PIPELINE(name, start, end, amount_adder, ss) \
  auto prod_##name = std::make_shared<ProducerInt>(start, end); \
  auto adders_##name = std::make_shared<std::vector<std::shared_ptr<Handler<int>>>>(amount_adder); \
  for (size_t index = 0; index < amount_adder; index++) { \
    (*adders_##name)[index] = std::make_shared<AdderInt>(); \
  } \
  auto cons_##name = std::make_shared<PrinterInt>(ss); \
  auto pipeline_##name = std::make_shared<Pipeline<int>>(prod_##name, adders_##name, cons_##name);

std::string CreateExpectation(int start, int end) {
  std::stringstream ss;
  for (int i = start; i < end; i++) {
    ss << "consumed: " << i << '\n';
  }
  return std::move(ss.str());
}

TEST_CASE("Test NEW pipeline wrapper construct", "[run_pipeline]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 3;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  std::stringstream ss_a;
  std::stringstream ss_b;
  //auto simple_a = CreatePipeline(0, 3, 30, ss_a);
  //auto simple_b = CreatePipeline(100, 3, 30, ss_b);

  //std::vector<Pipeline<int>> pipelines{simple_a, simple_b};

  SECTION("simple pipeline with handler") {
    CREATE_PIPELINE(simple_a, 0, 3, 30, ss_a);

    REQUIRE_NOTHROW(RunPipeline<int>(thread_pool_executor, pipeline_simple_a));

    REQUIRE(ss_a.str() == CreateExpectation(30, 33));
  }

  SECTION("multiple pipelines with handler") {
    CREATE_PIPELINE(simple_a, 0, 3, 30, ss_a);
    CREATE_PIPELINE(simple_b, 100, 103, 30, ss_b);

    auto pipelines = std::make_shared<std::vector<std::shared_ptr<Pipeline<int>>>>();
    pipelines->push_back(pipeline_simple_a);
    pipelines->push_back(pipeline_simple_b);

    REQUIRE_NOTHROW(RunPipelines<int>(thread_pool_executor, pipelines));

    REQUIRE(ss_a.str() == CreateExpectation(30, 33));
    REQUIRE(ss_b.str() == CreateExpectation(130, 133));
  }

  SECTION("run long pipeline") {
    std::stringstream ss_c;
    CREATE_PIPELINE(simple_c, 0, 3, 90, ss_c);

    REQUIRE_NOTHROW(RunPipeline<int>(thread_pool_executor, pipeline_simple_c));

    REQUIRE(ss_c.str() == CreateExpectation(90, 93));
  }

  SECTION("multiple long pipelines with handler") {
    std::stringstream ss_d;
    std::stringstream ss_e;
    CREATE_PIPELINE(simple_d, 0, 3, 90, ss_d);
    CREATE_PIPELINE(simple_e, 100, 103, 90, ss_e);

    auto pipelines = std::make_shared<std::vector<std::shared_ptr<Pipeline<int>>>>();
    pipelines->push_back(pipeline_simple_d);
    pipelines->push_back(pipeline_simple_e);

    REQUIRE_NOTHROW(RunPipelines<int>(thread_pool_executor, pipelines));

    REQUIRE(ss_d.str() == CreateExpectation(90, 93));
    REQUIRE(ss_e.str() == CreateExpectation(190, 193));
  }
}

