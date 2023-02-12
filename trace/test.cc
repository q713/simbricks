/*
 * Copyright 2021 Max Planck Institute for Software Systems, and
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

class IntProducer : public sim::coroutine::producer<int> {
  int start_ = 0;

 public:
  IntProducer(int start) : sim::coroutine::producer<int>(), start_(start) {
  }

  virtual sim::coroutine::task<void> produce(sim::coroutine::unbuffered_single_chan<int>* tar_chan) override {
    for (int i = 0; i < 10 && tar_chan && tar_chan->is_open(); i += 2) {
      int msg = start_ + i;
      if (!co_await tar_chan->write(msg)) {
        //std::cout << "producer write failure, start=" << start_
        //          << ", msg=" << msg << std::endl;
        break;
      }
      //std::cout << "produced value, start=" << start_ << ", msg=" << msg
      //          << std::endl;
    }
    
    //std::cout << "int producer finished, start=" << start_ << std::endl;
    co_return;
  };
};

class IntConsumer : public sim::coroutine::consumer<int> {
 public:
  explicit IntConsumer() : sim::coroutine::consumer<int>() {
  }

  sim::coroutine::task<void> consume(sim::coroutine::unbuffered_single_chan<int>* src_chan) override {
    std::optional<int> msg;
    //std::cout << "start int consumer" << std::endl;
    for (msg = co_await src_chan->read(); src_chan && msg;
         msg = co_await src_chan->read()) {
      std::cout << "consumed the following value: " << msg.value() << std::endl;
    }
    //if (!src_chan) {
    //  std::cout << "the source channel of the consumer is gone" << std::endl;
    //} else if (!msg) {
    //  std::cout << "received a empty message" << std::endl;
    //}
    //std::cout << "int consumer finished" << std::endl;
    co_return;
  };
};

class IntAdder : public sim::coroutine::pipe<int> {
 public:
  sim::coroutine::task<void> process(sim::coroutine::unbuffered_single_chan<int>* src_chan,
                     sim::coroutine::unbuffered_single_chan<int>* tar_chan) override {
    std::optional<int> msg;
    for (msg = co_await src_chan->read(); msg;
         msg = co_await src_chan->read()) {
      int m = msg.value();
      m = m + 10;
      if (!(co_await tar_chan->write(m))) {
        //std::cout << "int adder write failure" << std::endl;
        break;
      }
    }
    //std::cout << "exited int adder" << std::endl;
    co_return;
  }
};

struct dummy : public sim::coroutine::pipe<int> {
    sim::coroutine::task<void> process(
        sim::coroutine::unbuffered_single_chan<int>* src_chan,
        sim::coroutine::unbuffered_single_chan<int>* tar_chan) {
      std::optional<int> msg;
      for (msg = co_await src_chan->read(); msg;
           msg = co_await src_chan->read()) {
        int val = msg.value() - 10;
        if (!(co_await tar_chan->write(val))) {
          co_return;
        }
      }
      co_return;
    }
};

int main(int argc, char* argv[]) {
  IntProducer prodA{0};
  IntProducer prodB{0};
  IntProducer prodC{0};
  IntProducer prodD{0};
  IntConsumer cons;
  IntAdder adder;
  dummy d;
  sim::coroutine::collector<int> col{{prodA, prodB, prodC, prodD}};
  sim::coroutine::pipeline<int> pip{col, {adder, adder, adder}};
  if (sim::coroutine::awaiter<int>::await_termination(pip, cons)) {
    std::cout << "awaiter finished successful" << std::endl;
  } else {
    std::cout << "awaiter finished with error!!" << std::endl;
  }

  return 0;
}