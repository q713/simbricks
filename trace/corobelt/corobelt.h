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

#ifndef SIM_COROBELT_H_
#define SIM_COROBELT_H_

#include <concepts>
#include <exception>
#include <functional>
#include <iostream>
#include <list>
#include <optional>
#include <queue>
#include <set>
#include <thread>
#include <vector>

#if defined(__clang__)  // clang is compiler
#include <experimental/coroutine>
using std::experimental::coroutine_handle;
using std::experimental::noop_coroutine;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
#else  // g++ is compiler
#include <coroutine>
using std::coroutine_handle;
using std::noop_coroutine;
using std::suspend_always;
using std::suspend_never;
#endif

namespace sim {

namespace corobelt {

template <typename value_type>
struct task;
template <>
struct task<void>;

template <typename value_type>
struct yield_task;

struct promise_base {
  std::exception_ptr exception_;

  suspend_always initial_suspend() const noexcept {
    return {};
  }

  suspend_always final_suspend() const noexcept {
    return {};
  }

  void unhandled_exception() {
    exception_ = std::current_exception();
  }
};

template <typename value_type>
struct value_task_promise : public promise_base {
  bool has_value_ = false;
  value_type value_;

  inline task<value_type> get_return_object() noexcept {
    return task<value_type>{
        coroutine_handle<value_task_promise<value_type>>::from_promise(*this)};
  };

  suspend_always return_value(value_type value) noexcept {
    value_ = value;
    has_value_ = true;
    return {};
  }
};

struct void_task_promise : public promise_base {
  task<void> get_return_object() noexcept;

  void return_void() noexcept {
  }
};

template <typename value_type>
struct yield_promise : public promise_base {
  value_type value_;

  yield_task<value_type> get_return_object() {
    return yield_task<value_type>(
        coroutine_handle<yield_promise<value_type>>::from_promise(*this));
  }

  suspend_always yield_value(value_type from) {
    value_ = from;
    return {};
  }

  void return_void() {
  }
};

template <typename promise_t>
struct task_base {
  using promise_type = promise_t;

  bool is_done() {
    return !handle_ || handle_.done();
  }

  bool is_not_done() {
    return !is_done();
  }

  void resume_handle() {
    if (is_not_done()) {
      handle_.resume();
    }
  }

  explicit task_base(coroutine_handle<promise_type> handle) : handle_(handle) {
  }

  task_base(const task_base<promise_t>& tb) = delete;

  task_base<promise_t>& operator=(const task_base<promise_t>& tb) = delete;

  explicit task_base(task_base<promise_t>&& tb)
      : handle_(std::exchange(tb.handle_, nullptr)) {
  }
  task_base<promise_t>& operator=(const task_base<promise_t>&& tb) {
    handle_ = std::exchange(tb.handle_, nullptr);
  };

  coroutine_handle<promise_type> handle_;
};

template <typename value_type>
struct task : public task_base<value_task_promise<value_type>> {
  value_type return_value() noexcept {
    value_task_promise<value_type> promise = this->handle_.promise();
    if (promise.exception_) {
      std::rethrow_exception(promise.exception_);
    }
    return promise.value_;
  }

  task(coroutine_handle<value_task_promise<value_type>> handle)
      : task_base<value_task_promise<value_type>>(handle) {
  }

  task(const task<value_type>& t) = delete;
};

template <>
struct task<void> : public task_base<void_task_promise> {
  task(coroutine_handle<void_task_promise> handle)
      : task_base<void_task_promise>(handle) {
  }

  task(const task<void>& t) = delete;
};

inline task<void> void_task_promise::get_return_object() noexcept {
  return task<void>{coroutine_handle<void_task_promise>::from_promise(*this)};
};

template <typename value_type>
struct yield_task : public task_base<yield_promise<value_type>> {
  yield_task(coroutine_handle<yield_promise<value_type>> handle)
      : task_base<yield_promise<value_type>>(handle) {
  }

  yield_task(const yield_task<value_type>& yt) = delete;

  yield_task<value_type> operator=(const yield_task<value_type>& yt) = delete;

  yield_task(yield_task<value_type>&& yt)
      : task_base<yield_promise<value_type>>(std::move(yt)),
        has_value_(yt.has_value_) {
  }

  yield_task<value_type> operator=(yield_task<value_type>&& yt) {
    this->handle_ = std::exchange(yt.handle_, nullptr);
    has_value_ = yt.has_value_;
  }

  ~yield_task() {
    if (this->handle_)
      this->handle_.destroy();
  }

  explicit operator bool() {
    retrieve_val();
    return this->is_not_done();
  }

  value_type get() {
    retrieve_val();
    has_value_ = false;
    return this->handle_.promise().value_;
  }

  bool has_value_ = false;

 private:
  void retrieve_val() {
    if (this->is_not_done() and not has_value_) {
      this->resume_handle();
      if (this->handle_.promise().exception_) {
        std::rethrow_exception(this->handle_.promise().exception_);
      }
      has_value_ = true;
    }
  }
};

template <typename T>
struct producer {
  explicit producer() = default;
  virtual yield_task<T> produce() {
    co_return;
  };
};

template <typename T>
struct consumer {
  explicit consumer() = default;
  virtual task<void> consume(yield_task<T>* producer_task) {
    co_return;
  };
};

template <typename T>
struct pipe {
  explicit pipe() = default;
  virtual yield_task<T> process(yield_task<T>* producer_task) {
    co_return;
  }
};

template <typename value_type>
struct pipeline : public producer<value_type> {
 private:
  producer<value_type>& producer_;
  std::vector<std::reference_wrapper<pipe<value_type>>> pipes_;

 public:
  explicit pipeline(producer<value_type>& p,
                    std::vector<std::reference_wrapper<pipe<value_type>>> pipes)
      : producer<value_type>(), producer_(p), pipes_(std::move(pipes)) {
  }

  yield_task<value_type> produce() override {
    if (pipes_.size() < 1) {
      co_return;
    }

    // create and start all generators
    yield_task<value_type> producer_task = producer_.produce();
    if (not producer_task) {
      // std::cout << "pipeline: init producer error" << std::endl;
      co_return;
    }

    std::list<yield_task<value_type>> pipe_tasks;
    yield_task<value_type>* last_task = &producer_task;
    for (pipe<value_type>& p : pipes_) {
      pipe_tasks.emplace_back(p.process(last_task));
      // std::cout << "get pointer from vector" << std::endl;
      last_task = &pipe_tasks.back();

      if (not last_task or not *last_task) {
        // std::cout << "pipeline: init pipe error" << std::endl;
        co_return;
      }
    }

    // std::cout << "pipeline: consume till finished" << std::endl;
    while (last_task and *last_task) {
      // std::cout << "pipeline: get first value" << std::endl;
      value_type val = last_task->get();
      co_yield val;
    }

    // std::cout << "pipeline: leaf pipeline" << std::endl;
    co_return;
  };
};

template <typename value_type, typename compare = std::greater<value_type>>
struct collector : public producer<value_type> {
 private:
  struct comperator {
   private:
    compare compare_;

   public:
    inline bool operator()(
        const std::pair<value_type, yield_task<value_type>*>& le,
        const std::pair<value_type, yield_task<value_type>*>& re) const {
      return compare_(le.first, re.first);
    }
  };

  std::vector<std::reference_wrapper<producer<value_type>>> producer_;

 public:
  explicit collector(
      std::vector<std::reference_wrapper<producer<value_type>>> producers)
      : producer<value_type>(), producer_(std::move(producers)) {
  }

  yield_task<value_type> produce() override {
    if (producer_.size() < 2) {
      co_return;
    }

    std::list<yield_task<value_type>> tasks;
    std::priority_queue<
        std::pair<value_type, yield_task<value_type>*>,
        std::vector<std::pair<value_type, yield_task<value_type>*>>, comperator>
        sources;
    // std::cout << "collector: start init of sources" << std::endl;
    for (auto it = producer_.begin(); it < producer_.end(); it++) {
      // std::cout << "collector: try to init next producer" << std::endl;
      producer<value_type>& p = *it;
      tasks.emplace_back(std::move(p.produce()));
      yield_task<value_type>* producer_task = &tasks.back();
      // std::cout << "started next producer" << std::endl;
      if (not producer_task or not *producer_task) {
        // std::cout << "collector: init sources error" << std::endl;
        continue;
      }
      value_type val = producer_task->get();
      // std::cout << "collector: init retrieved value" << std::endl;
      sources.push(std::make_pair(val, producer_task));
      // std::cout << "collector: init added pair" << std::endl;
    }

    // std::cout << "collector: start consuming sources" << std::endl;
    while (!sources.empty()) {
      auto val_task_pair = sources.top();
      sources.pop();

      yield_task<value_type>* next_task = val_task_pair.second;
      value_type val = val_task_pair.first;
      co_yield val;

      if (not next_task or not *next_task) {
        continue;
      }

      val = next_task->get();
      sources.push(std::make_pair(val, next_task));
    }

    co_return;
  }
};

template <typename value_type>
struct awaiter {
  static bool await_termination(producer<value_type>& producer) {
    do_nothing_consumer c;
    return await_termination(producer, c);
  }

  static bool await_termination(producer<value_type>& producer,
                                consumer<value_type>& consumer) {
    yield_task<value_type> producer_task = producer.produce();
    task<void> consumer_task = consumer.consume(&producer_task);

    while (consumer_task.is_not_done()) {
      consumer_task.resume_handle();
    }

    return true;
  }

 private:
  struct do_nothing_consumer : public consumer<value_type> {
    task<void> consume(yield_task<value_type>* producer_task) override {
      if (not producer_task) {
        co_return;
      }
      while (*producer_task) {
        producer_task->get();
      }
      co_return;
    }
  };
};

template <typename source_type, typename target_type>
struct transformer : public producer<target_type> {
  virtual target_type transform(source_type src) {
    return {};
  }

  yield_task<target_type> produce() override {
    yield_task<source_type> source_task = prod_.produce();

    while (source_task) {
      source_type src = source_task.get();
      target_type next = transform(src);
      co_yield next;
    }

    co_return;
  }

  transformer(producer<source_type>& prod)
      : producer<target_type>(), prod_(prod) {
  }

 protected:
  producer<source_type>& prod_;
};

};  // namespace corobelt

};  // namespace sim

#endif  // SIM_COROBELT_H_
