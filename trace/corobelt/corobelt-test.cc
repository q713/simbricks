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
#include <optional>

#include "trace/corobelt/corobelt.h"

struct int_prod : public sim::corobelt::producer<int> {
  int start = 0;

  int_prod(int s) : producer<int>(), start(s) {
  }

  sim::corobelt::yield_task<int> produce() override {
    for (int i = start; i < 10 + start; i++) {
      co_yield i;
      // std::cout << "producer: yielded value: " << i << std::endl;
    }

    co_return;
  };
};

struct int_str_trans : public sim::corobelt::transformer<int, std::string> {
  std::string transform(int src) override {
    return std::to_string(src);
  }

  int_str_trans(sim::corobelt::producer<int> &prod)
      : sim::corobelt::transformer<int, std::string>(prod) {
  }
};

struct int_cons : public sim::corobelt::consumer<int> {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<int> *producer_task) override {
    if (not producer_task) {
      co_return;
    }

    while (*producer_task) {
      int val = producer_task->get();
      std::cout << "consumed: " << val << std::endl;
    }

    co_return;
  };
};

struct str_cons : public sim::corobelt::consumer<std::string> {
  sim::corobelt::task<void> consume(
      sim::corobelt::yield_task<std::string> *producer_task) override {
    if (not producer_task) {
      co_return;
    }

    while (*producer_task) {
      std::string val = producer_task->get();
      std::cout << "consumed string: " << val << std::endl;
    }

    co_return;
  };
};

struct int_adder : public sim::corobelt::pipe<int> {
  sim::corobelt::yield_task<int> process(
      sim::corobelt::yield_task<int> *producer_task) override {
    if (not producer_task) {
      // std::cout << "adder: immediately leaf" << std::endl;
      co_return;
    }

    while (*producer_task) {
      int val = producer_task->get();
      val += 1;
      co_yield val;
      // std::cout << "adder: yielded value" << val << std::endl;
    }

    // std::cout << "adder: leaf adder" << std::endl;
    co_return;
  }
};

struct str_adder : public sim::corobelt::pipe<std::string> {
  sim::corobelt::yield_task<std::string> process(
      sim::corobelt::yield_task<std::string> *producer_task) override {
    if (not producer_task) {
      co_return;
    }

    while (*producer_task) {
      std::string val = producer_task->get();
      val += " + 1";
      co_yield val;
    }

    co_return;
  }
};

sim::corobelt::yield_task<int> produce(int start) {
  for (int i = start; i < 10 + start; i++) {
    co_yield i;
    // std::cout << "produced: " << i << std::endl;
  }
  co_return;
}

sim::corobelt::yield_task<int> nested_produce(int start) {
  sim::corobelt::yield_task<int> gen = produce(start);
  while (gen) {
    int next = gen.get();
    co_yield next;
  }
}

int main(int argc, char *argv[]) {
  {
    sim::corobelt::yield_task<int> t = produce(0);
    while (t) {
      std::cout << "received: " << t.get() << std::endl;
    }
  }

  std::cout << "================ next ================" << std::endl;

  {
    sim::corobelt::yield_task<int> t = nested_produce(0);
    while (t) {
      std::cout << "received: " << t.get() << std::endl;
    }
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_cons i_consumer;
    sim::corobelt::collector<int> collector{{producerA, producerB}};
    sim::corobelt::awaiter<int>::await_termination(collector, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_cons i_consumer;
    sim::corobelt::collector<int> collector{{producerA, producerB}};
    sim::corobelt::pipeline<int> pipeline{collector, {adderA}};
    sim::corobelt::awaiter<int>::await_termination(pipeline, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    sim::corobelt::pipeline<int> pipeline{producerA, {adderA, adderB}};
    sim::corobelt::awaiter<int>::await_termination(pipeline, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    sim::corobelt::collector<int> collector{{producerA, producerB}};
    sim::corobelt::pipeline<int> pipeline{collector, {adderA, adderB}};
    sim::corobelt::awaiter<int>::await_termination(pipeline, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    sim::corobelt::pipeline<int> pipelineA{producerA, {adderA}};
    sim::corobelt::pipeline<int> pipelineB{producerB, {adderB}};
    sim::corobelt::collector<int> collector{{pipelineA, pipelineB}};
    sim::corobelt::awaiter<int>::await_termination(collector, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    sim::corobelt::pipeline<int> pipelineA{producerA, {adderA}};
    sim::corobelt::pipeline<int> pipelineB{pipelineA, {adderB}};
    sim::corobelt::awaiter<int>::await_termination(pipelineB, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_adder adderA;
    str_cons s_consumer;
    sim::corobelt::pipeline<int> pipelineA{producerA, {adderA}};
    int_str_trans transf{pipelineA};
    sim::corobelt::awaiter<std::string>::await_termination(transf,
                                                           s_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_adder adderA;
    str_adder adderB;
    str_cons s_consumer;
    sim::corobelt::pipeline<int> pipelineA{producerA, {adderA}};
    int_str_trans transf{pipelineA};
    sim::corobelt::pipeline<std::string> pipelineB{transf, {adderB}};
    sim::corobelt::awaiter<std::string>::await_termination(pipelineB,
                                                           s_consumer);
  }

  return 0;
}