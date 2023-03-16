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

#include "trace/corobelt/coroutine.h"

struct int_prod : public sim::coroutine::producer<int> {
  int start = 0;

  int_prod(int s) : sim::coroutine::producer<int>(), start(s) {
  }

  sim::coroutine::task<void> produce(
      sim::coroutine::unbuffered_single_chan<int>* tar_chan) override {
    for (int i = start; i < start + 1'000'000; i++) {
      if (!co_await tar_chan->write(i)) {
        std::cout << "production failure" << std::endl;
        co_return;
      }
      // std::cout << "produced: " << i << std::endl;
    }
    co_return;
  };
};

struct int_cons : public sim::coroutine::consumer<int> {
  sim::coroutine::task<void> consume(
      sim::coroutine::unbuffered_single_chan<int>* src_chan) override {
    std::optional<int> msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      std::cout << msg.value() << std::endl;
    }
    co_return;
  }
};

struct str_cons : public sim::coroutine::consumer<std::string> {
  sim::coroutine::task<void> consume(
      sim::coroutine::unbuffered_single_chan<std::string>* src_chan) override {
    std::optional<std::string> msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      std::cout << "consumed " << msg.value() << std::endl;
    }
    co_return;
  }
};

/*
struct int_str_transformer : public sim::coroutine::transformer<int,
std::string> {

  std::optional<std::string> transform(int value) override {
    return "transformed: " + std::to_string(value);
  }

  int_str_transformer(sim::coroutine::producer<int> &p)
    : sim::coroutine::transformer<int, std::string>(p) {
  }

};
*/

struct int_adder : public sim::coroutine::pipe<int> {
  sim::coroutine::task<void> increment_and_writeback(
      int to_increment, sim::coroutine::unbuffered_single_chan<int>* tar_chan) {
    to_increment += 1;
    co_await tar_chan->write(to_increment);
    co_return;
  }

  sim::coroutine::task<int> increment(int to_increment) {
    to_increment += 1;
    co_return to_increment;
  }

  sim::coroutine::task<int> read_increment(
      sim::coroutine::unbuffered_single_chan<int>* src_chan) {
    std::optional<int> msg = co_await src_chan->read();
    if (msg) {
      co_return msg.value() + 1;
    }
    co_return -1;
  }

  sim::coroutine::task<void> process(
      sim::coroutine::unbuffered_single_chan<int>* src_chan,
      sim::coroutine::unbuffered_single_chan<int>* tar_chan) override {
    std::optional<int> msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      int next = msg.value();
      next += 1;
      // sim::coroutine::task<int> task = increment(next);
      // next = sim::coroutine::retrieve_val(task);
      // increment_and_writeback(next, tar_chan);
      if (!co_await tar_chan->write(next)) {
        std::cout << "adder failure" << std::endl;
      }
      // std::cout << "adder produced " << next << std::endl;
    }
    /*while(not src_chan->is_closed()) {
      sim::coroutine::task<int> task = read_increment(src_chan);
      int next = sim::coroutine::retrieve_val(task); // TODO: fix this
      co_await tar_chan->write(next);
    }
    */
    co_return;
  }
};

int main(int argc, char* argv[]) {
  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_cons i_consumer;
    sim::coroutine::collector<int> collector{{producerA, producerB}};
    sim::coroutine::awaiter<int>::await_termination(collector, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_cons i_consumer;
    sim::coroutine::collector<int> collector{{producerA, producerB}};
    sim::coroutine::pipeline<int> pipeline{collector, {adderA}};
    sim::coroutine::awaiter<int>::await_termination(pipeline, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    sim::coroutine::collector<int> collector{{producerA, producerB}};
    sim::coroutine::pipeline<int> pipeline{collector, {adderA, adderB}};
    sim::coroutine::awaiter<int>::await_termination(pipeline, i_consumer);
  }

  std::cout << "================ next ================" << std::endl;

  {
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    sim::coroutine::pipeline<int> pipelineA{producerA, {adderA}};
    sim::coroutine::pipeline<int> pipelineB{producerB, {adderB}};
    sim::coroutine::collector<int> collector{{pipelineA, pipelineB}};
    sim::coroutine::awaiter<int>::await_termination(collector, i_consumer);
  }

  /*
    int_prod producerA{0};
    int_prod producerB{1};
    int_adder adderA;
    int_adder adderB;
    int_cons i_consumer;
    str_cons s_consumer;


    //sim::coroutine::pipeline<int> pipelineA{producerA, {adderA}};
    //sim::coroutine::pipeline<int> pipelineB{producerB, {adderB}};

    //sim::coroutine::collector<int> collector{{pipelineA, pipelineB}};
    sim::coroutine::collector<int> collector{{producerA, producerB}};
    //sim::coroutine::pipeline<int> pipeline{collector, {adderA}};

    //int_str_transformer transfrmr{pipeline};
    //int_str_transformer transfrmrA{pipelineA};
    //int_str_transformer transfrmrB{pipelineB};

    //sim::coroutine::collector<std::string> collector{{transfrmrA,
    transfrmrB}}; // TODO: fix this

    //sim::coroutine::awaiter<std::string>::await_termination(transfrmr,
    s_consumer);
    //sim::coroutine::awaiter<std::string>::await_termination(transfrmrA,
    s_consumer);
    //sim::coroutine::awaiter<std::string>::await_termination(collector,
    s_consumer);
    //sim::coroutine::awaiter<int>::await_termination(pipeline, i_consumer);
    sim::coroutine::awaiter<int>::await_termination(collector, i_consumer);
    //sim::coroutine::awaiter<int>::await_termination(producerA, i_consumer);
    */

  return 0;
}
