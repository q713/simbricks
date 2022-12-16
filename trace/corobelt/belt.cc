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

#include "trace/corobelt/belt.h"

corobelt::col_key_t<T> corobelt::Collector::pop() {
  auto e_src_pair_it = sources_.begin();
  col_key_t<T> e_src_pair(*e_src_pair_it);
  sources_.erase(e_src_pair_it);
  return e_src_pair;
}

bool corobelt::Collector<T>::move(corobelt::coro_pull_t<T> *src) {
  if (is_finished<T>(src)) {
    // cleanup heap on the fly
    if (src != nullptr) {
      delete src;
    }
    return false;
  }

  T produced = src->get();
  (*src)();

  auto res = sources_.insert(std::make_pair(produced, src));
  return res.second;
}

void corobelt::Collector<T>::produce(corobelt::coro_push_t<T> &sink) override {
  if (producer_.empty()) {
    // std::cout << "no producers given" << std::endl;
    return;
  }
  // TODO: ERROR HANDLING AND CHECKS!!!!!!!!!!!!!!

  // create producer coroutines
  for (corobelt::Producer<T> *prod : producer_) {
    corobelt::coro_pull_t<T> *src = new corobelt::coro_pull_t<T>(std::bind(
        &corobelt::Producer<T>::produce, prod, std::placeholders::_1));
    if (!move(src)) {
      // std::cout << "coroutines doesnt create events" << std::endl;
    }
  }

  // process events in order
  while (!sources_.empty()) {
    corobelt::col_key_t<T> e_src_pair = std::move(pop());

    // pass event further
    T event = e_src_pair.first;
    sink(event);

    // if coroutine insert next
    corobelt::coro_pull_t<T> *src = e_src_pair.second;
    if (!move(src)) {
      // TODO: debug print for coroutine termination info
      // std::cout << "finished a coroutine" << std::endl;
    }
  }

  return;
}

bool corobelt::Awaiter<T>::await_termination() {
  if (producer_ == nullptr) {
    // TODO: print error
    return false;
  }

  // create producer
  corobelt::coro_pull_t<T> producer(std::bind(
      &corobelt::Producer<T>::produce, producer_, std::placeholders::_1));
  if (!producer) {
    // TODO: print error
    return false;
  }

  corobelt::coro_push_t<T> *consumer_ptr = nullptr;

  // create potentielly consumer
  if (consumer_ != nullptr) {
    consumer_ptr = new coro_push_t<T>(std::bind(
        &corobelt::Consumer<T>::consume, consumer_, std::placeholders::_1));
    if (is_finished<T>(consumer_ptr)) {
      if (consumer_ptr != nullptr) {
        delete consumer_ptr;
      }
      // TODO: error print
      return false;
    }
  }

  for (T event : producer) {
    if (consumer_ptr != nullptr) {
      (*consumer_ptr)(event);
    }
  }

  return true;
}

bool corobelt::Pipeline<T>::deposite(corobelt::coro_pull_t<T> *to_dump) {
  if (is_finished<T>(to_dump)) {
    return false;
  }
  auto res = landfill_.insert(to_dump);
  if (!res.second) {
    return false;
  }
  return true;
}

void corobelt::Pipeline<T>::clearLandfill() {
  for (auto it = landfill_.begin(); it != landfill_.end(); it++) {
    corobelt::coro_pull_t<T> *src = *it;
    if (src != nullptr) {
      delete src;
    }
  }
  landfill_.clear();
}

void corobelt::Pipeline<T>::produce(corobelt::coro_push_t<T> &sink) override {
  if (source_ == nullptr) {
    // TODO: error print
    return;
  }

  corobelt::coro_pull_t<T> *provider_ptr;

  provider_ptr = new corobelt::coro_pull_t<T>(std::bind(
      &corobelt::Producer<T>::produce, source_, std::placeholders::_1));
  if (!deposite(provider_ptr)) {
    if (provider_ptr != nullptr) {
      delete provider_ptr;
    }
    // TODO: error print
    return;
  }

  for (Pipe<T> *pipe : pipes_) {
    provider_ptr = new corobelt::coro_pull_t<T>(
        std::bind(&Pipe<T>::process, pipe, std::placeholders::_1,
                  std::ref(*provider_ptr)));
    if (!deposite(provider_ptr)) {
      if (provider_ptr != nullptr) {
        delete provider_ptr;
      }
      clearLandfill();
      return;
    }
  }

  for (T event : *provider_ptr) {
    sink(event);
  }

  clearLandfill();
  return;
}

#endif  // SIMBRICKS_TRACE_COROBELT_H_
