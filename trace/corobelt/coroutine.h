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

#ifndef SIM_COROUTINE_H_
#define SIM_COROUTINE_H_

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <iostream>
#include <list>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <thread>

#include "lib/utils/log.h"

namespace sim {

namespace coroutine {

//#define SIM_COROUTINE_DEBUG_ 1

#ifdef SIM_COROUTINE_DEBUG_

#define COROBELT_LOG_ERR_(m) DLOGERR(m);

#define COROBELT_LOG_WARN_(m) DLOGWARN(m);

#define COROBELT_LOG_INF_(m) DLOGIN(m);

#else

#define COROBELT_LOG_ERR_(m) \
  do {                       \
  } while (0);

#define COROBELT_LOG_WARN_(m) \
  do {                        \
  } while (0);

#define COROBELT_LOG_INF_(m) \
  do {                       \
  } while (0);

#endif

/*
template <typename T>
struct generator {
  // forward declaration
  struct generator_promise;
  using promise_type = struct generator_promise;
  using handle_type = std::coroutine_handle<promise_type>;

 private:
  handle_type handle_;
  bool has_value_ = false;

  void compute_val() {
    if (!has_value_) {
      handle_();
      if (handle_.promise().exception_)
        std::rethrow_exception(handle_.promise().exception_);
      has_value_ = true;
    }
  }

 public:
  struct generator_promise {
    T value_;
    std::exception_ptr exception_;

    generator get_return_object() {
      return generator(handle_type::from_promise(*this));
    }

    std::suspend_always initial_suspend() {
      return {};
    }

    std::suspend_always final_suspend() noexcept {
      return {};
    }

    void unhandled_exception() {
      exception_ = std::current_exception();
    }

    template <std::convertible_to<T> From>
    std::suspend_always yield_value(From&& from) {
      value_ = std::forward<From>(from);
      return {};
    }

    void return_void() {
    }
  };

  generator(handle_type handle) : handle_(handle) {
  }

  ~generator() {
    if (handle_)
      handle_.destroy();
  }

  bool is_done() {
    compute_val();
    return handle_.done();
  }

  bool is_not_done() {
    return !is_done();
  }

  T retrieve_val() {
    compute_val();
    has_value_ = false;
    return std::move(handle_.promise().value_);
  }

  explicit operator bool() {
    return is_not_done();
  }

  T operator()() {
    return retrieve_val();
  }
};

struct [[nodiscard]] task {
  // forward declaration + needed compiler hint
  struct task_promise;
  using promise_type = task_promise;

  explicit task(std::coroutine_handle<task_promise> handle) : handle_(handle) {
  }

  struct task_promise {
    task get_return_object() noexcept {
      return task{std::coroutine_handle<task_promise>::from_promise(*this)};
    };

    std::suspend_never initial_suspend() const noexcept {
      return {};
    }
    std::suspend_never final_suspend() const noexcept {
      return {};
    }

    void return_void() noexcept {
    }

    void unhandled_exception() noexcept {
      std::cerr << "Unhandled exception caught...\n";
      exit(1);
    }
  };

 private:
  std::coroutine_handle<task_promise> handle_;
};

struct dispatcher {
  using handle_t = std::coroutine_handle<>;

  struct awaiter {
    dispatcher* executor_;

    explicit awaiter(dispatcher* exe) : executor_(exe) {
    }

    constexpr bool await_ready() const noexcept {
      return false;
    }

    constexpr void await_resume() const noexcept {
    }

    void await_suspend(handle_t coro) const noexcept {
      executor_->enqueue_task(coro);
    }
  };

  auto schedule() {
    return awaiter{this};
  }

  explicit dispatcher(const std::size_t threadCount)
      : is_pool_activated_(true) {
    for (std::size_t i = 0; i < threadCount; ++i) {
      std::thread worker_thread([this]() { this->thread_loop(); });
      worker_.push_back(std::move(worker_thread));
    }
  }

  ~dispatcher() {
    shutdown();
  }

 private:
  // NOTE: do not concurrently access the worker list
  std::list<std::thread> worker_;

  bool is_pool_activated_;
  std::mutex dispatcher_guard_;
  std::condition_variable wait_for_tasks_;
  std::queue<handle_t> waiting_tasks_;

  void thread_loop() {
    while (is_pool_activated_) {
      std::unique_lock<std::mutex> lock(dispatcher_guard_);

      while (waiting_tasks_.empty() && is_pool_activated_) {
        wait_for_tasks_.wait(lock);
      }

      if (!is_pool_activated_) {
        return;
      }

      auto coro = waiting_tasks_.front();
      waiting_tasks_.pop();
      lock.unlock();

      coro.resume();
    }
  }

  void enqueue_task(handle_t coro) noexcept {
    std::unique_lock<std::mutex> lock(dispatcher_guard_);
    waiting_tasks_.emplace(coro);
    wait_for_tasks_.notify_one();
  }

  void shutdown() {
    std::unique_lock<std::mutex> lock(dispatcher_guard_);
    is_pool_activated_ = false;
    lock.unlock();

    wait_for_tasks_.notify_all();
    while (worker_.size() > 0) {
      std::thread& thread = worker_.back();
      if (thread.joinable()) {
        thread.join();
      }
      worker_.pop_back();
    }
  }
};

task run_async_print(std::string s, dispatcher& pool) {
  co_await pool.schedule();
  std::cout << "This is a hello from thread: \t\t" << std::this_thread::get_id()
            << std::endl;
}

*/

// forward declaration of the task object
template <typename value_type>
struct task;

template <>
struct task<void>;

template <typename value_type>
struct value_task_promise {
  bool has_value_ = false;
  value_type value_;

  inline task<value_type> get_return_object() noexcept {
    return task<value_type>{
        std::coroutine_handle<value_task_promise<value_type>>::from_promise(
            *this)};
  };

  std::suspend_never initial_suspend() const noexcept {
    return {};
  }

  std::suspend_always final_suspend() const noexcept {
    return {};
  }

  std::suspend_always return_value(value_type value) noexcept {
    value_ = value;
    has_value_ = true;
    return {};
  }

  void unhandled_exception() noexcept {
    std::cerr << "Unhandled exception caught...\n";
    exit(1);
  }
};

struct void_task_promise {
  task<void> get_return_object() noexcept;

  std::suspend_never initial_suspend() const noexcept {
    return {};
  }

  std::suspend_always final_suspend() const noexcept {
    return {};
  }

  void return_void() noexcept {
  }

  void unhandled_exception() noexcept {
    std::cerr << "Unhandled exception caught...\n";
    exit(1);
  }
};

template <typename promise_t>
struct task_base {
  using promise_type = promise_t;

  bool is_done() {
    return destroyed_ || !handle_ || handle_.done();
  }

  bool is_not_done() {
    return !is_done();
  }

  void resume_handle() {
    if (is_not_done()) {
      handle_.resume();
    }
  }

  void destroy() {
    destroyed_ = true;
    if (handle_) {
      handle_.destroy();
    }
  }

  explicit task_base(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {
  }

 protected:
  bool destroyed_ = false;
  std::coroutine_handle<promise_type> handle_;
};

template <typename value_type>
struct task : public task_base<value_task_promise<value_type>> {
  value_type return_value() noexcept {
    return this->handle_.promise().value_;
  }

  explicit task(std::coroutine_handle<value_task_promise<value_type>> handle)
      : task_base<value_task_promise<value_type>>(handle) {
  }
};

template <>
struct task<void> : public task_base<void_task_promise> {
  explicit task(std::coroutine_handle<void_task_promise> handle)
      : task_base<void_task_promise>(handle) {
  }
};

inline task<void> void_task_promise::get_return_object() noexcept {
  return task<void>{
      std::coroutine_handle<void_task_promise>::from_promise(*this)};
};

// TODO: adapt for multithreading + buffered version + multi consumer/producer
template <typename T>
struct unbuffered_single_chan {
  struct chan_reader {
    unbuffered_single_chan<T>* chan_;

    bool await_ready() const noexcept {
      COROBELT_LOG_INF_("enter await_ready chan_reader\n");
      if (!chan_->has_val_ || !chan_->writer_resumable_) {
        return false;
      }
      std::swap(chan_->reader_resumable_, chan_->writer_resumable_);
      COROBELT_LOG_INF_("exit await_ready chan_reader\n");
      return true;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<void> coro) const noexcept {
      COROBELT_LOG_INF_("enter await_suspend chan_reader\n");
      if (!chan_) {
        COROBELT_LOG_ERR_("await_suspend chan_reader channel is gone\n");
        return std::noop_coroutine();
      }
      chan_->reader_resumable_ = coro;
      if (!chan_->has_val_ && chan_->writer_resumable_) {
        auto resumable = chan_->writer_resumable_;
        chan_->writer_resumable_ = nullptr;
        if (resumable && !resumable.done()) {
          return resumable;
        }
        if (chan_ && chan_->reader_resumable_ &&
            !chan_->reader_resumable_.done()) {
          return chan_->reader_resumable_;
        }
      }
      return std::noop_coroutine();
    }

    std::optional<T> await_resume() const noexcept {
      COROBELT_LOG_INF_("enter await_resume chan_reader\n");
      if (!chan_) {
        COROBELT_LOG_ERR_("exit await_resume channel is null\n");
        return std::nullopt;
      }

      if (chan_->is_closed_) {
        COROBELT_LOG_INF_("exit await_resume chan_reader closed channel\n");
        return std::nullopt;
      }

      if (!chan_->has_val_) {
        COROBELT_LOG_WARN_("await resume, but no value in channel\n");
        return std::nullopt;
      }
      chan_->has_val_ = false;
      COROBELT_LOG_INF_("exit await_resume chan_reader\n");
      return std::optional<T>{chan_->value_};
    }

    explicit chan_reader(unbuffered_single_chan<T>* chan) : chan_(chan) {
    }
  };

  struct chan_writer {
    T value_to_write_;
    unbuffered_single_chan<T>* chan_;

    bool await_ready() const noexcept {
      COROBELT_LOG_INF_("enter await_ready chan_writer\n");
      if (chan_->has_val_ || !chan_->reader_resumable_) {
        return false;
      }
      std::swap(chan_->reader_resumable_, chan_->writer_resumable_);
      COROBELT_LOG_INF_("exit await_ready chan_writer\n");
      return true;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<void> coro) const noexcept {
      COROBELT_LOG_INF_("enter await_suspend chan_writer\n");
      if (!chan_) {
        COROBELT_LOG_ERR_("await_suspend chan_writer channel is null\n");
        return std::noop_coroutine();
      }
      chan_->writer_resumable_ = coro;
      COROBELT_LOG_INF_("exit await_suspend chan_writer\n");
      return std::noop_coroutine();
    }

    bool await_resume() const noexcept {
      COROBELT_LOG_INF_("enter await_resume chan_writer\n");
      if (!chan_) {
        COROBELT_LOG_ERR_("await_resume chan_writer channel is gone\n");
        return false;
      }

      if (chan_->is_closed_) {
        COROBELT_LOG_INF_("await_resume chan_writer channel is closed\n");
        return false;
      }

      chan_->has_val_ = true;
      chan_->value_ = value_to_write_;
      if (chan_->writer_resumable_ && !chan_->is_closed_) {
        auto resumable = chan_->writer_resumable_;
        chan_->writer_resumable_ = nullptr;
        if (resumable && !resumable.done()) {
          resumable.resume();
        }
      }
      COROBELT_LOG_INF_("exit await_resume chan_writer\n");
      return true;
    }

    chan_writer(unbuffered_single_chan<T>* chan) : chan_(chan) {
    }
  };

 private:
  friend chan_reader;
  friend chan_writer;

  chan_writer writer_;
  chan_reader reader_;

  std::coroutine_handle<void> reader_resumable_;
  std::coroutine_handle<void> writer_resumable_;

  bool is_closed_ = false;
  bool has_val_ = false;
  T value_;

 public:
  explicit unbuffered_single_chan()
      : writer_(this), reader_(this), is_closed_(false), has_val_(false) {
  }

  chan_writer write(T& value) {
    writer_.value_to_write_ = value;
    return std::move(writer_);
  }

  chan_reader read() {
    return std::move(reader_);
  }

  bool is_open() {
    return !is_closed_;
  }

  bool is_closed() {
    return is_closed_;
  }

  void close_chan() {
    is_closed_ = true;
  }

  ~unbuffered_single_chan() {
    COROBELT_LOG_INF_("start channel destruction\n");
    // TODO: raii
    is_closed_ = true;
    COROBELT_LOG_INF_("finished channel destruction\n");
  }
};

template<typename T>
struct pipeline;

template <typename T, typename compare>
struct collector;

template<typename T>
struct awaiter;

template <typename T>
struct producer {
  explicit producer() = default;

  virtual task<void> produce(unbuffered_single_chan<T>* tar_chan) {
    co_return;
  };
};

template <typename T>
struct consumer {
  explicit consumer() = default;

  virtual task<void> consume(unbuffered_single_chan<T>* src_chan) {
    co_return;
  };
};

template <typename T>
struct pipe {
  explicit pipe() = default;

  virtual task<void> process(unbuffered_single_chan<T>* src_chan,
                             unbuffered_single_chan<T>* tar_chan) {
    co_return;
  }
};

template <typename T>
struct pipeline : public producer<T> {
 private:
  producer<T>& producer_;
  std::vector<std::reference_wrapper<pipe<T>>> pipes_;

  std::vector<unbuffered_single_chan<T>*> channels_;
  std::vector<task<void>> tasks_;
 
 public:
  explicit pipeline(producer<T>& producer,
                    std::vector<std::reference_wrapper<pipe<T>>> pipes)
      : producer<T>(), producer_(producer), pipes_(std::move(pipes)) {
  }

  ~pipeline() {
    COROBELT_LOG_INF_("start destruction of pipeline\n");
    for (task<void>& ta : tasks_) {
      ta.destroy();
    }

    for (unbuffered_single_chan<T>* chan : channels_) {
      if (chan) {
        delete chan;
      }
    }
    COROBELT_LOG_INF_("finished destruction of pipeline\n");
  }

  task<void> produce(unbuffered_single_chan<T>* tar_chan) override {
    COROBELT_LOG_INF_("start pipeline\n");

    if (pipes_.size() < 1) {
      COROBELT_LOG_ERR_("You must pass at least one pipe to a pipeline\n");
      co_return;
    }

    unbuffered_single_chan<T>* src_chan;
    for (auto it = pipes_.crbegin(); it != pipes_.crend(); it++) {
      src_chan = new unbuffered_single_chan<T>{};
      if (!src_chan) {
        COROBELT_LOG_ERR_("pipeline initialization error\n");
        co_return;
      }
      channels_.push_back(src_chan);
      pipe<T>& p = *it;
      task<void> t = p.process(src_chan, tar_chan);
      tasks_.push_back(t);
      tar_chan = src_chan;
    }

    unbuffered_single_chan<T>* pipe_end = new unbuffered_single_chan<T>{};
    if (!pipe_end) {
      COROBELT_LOG_ERR_("pipeline allocation error\n");
      co_return;
    }
    channels_.push_back(pipe_end);
    task<void> t = producer_.produce(pipe_end);
    tasks_.push_back(t);

    std::optional<T> msg;
    
    do {
      for (msg = co_await pipe_end->read(); msg; msg = co_await pipe_end->read()) {
        if (!tar_chan || !co_await tar_chan->write(msg.value())) {
          COROBELT_LOG_ERR_("pipeline write failure\n");
          break;
        }
      }

      if (t.is_not_done())
        t.resume_handle();
    } while(t.is_not_done());

    COROBELT_LOG_INF_("leave pipeline\n");
    co_return;
  };
};

template <typename T, typename compare = std::greater<T>>
struct collector : public producer<T> {
 private:
  struct comperator {
   private:
    compare compare_;

   public:
    inline bool operator()(
        const std::pair<T, unbuffered_single_chan<T>*>& le,
        const std::pair<T, unbuffered_single_chan<T>*>& re) const {
      return compare_(le.first, re.first);
    }
  };

  std::vector<std::reference_wrapper<producer<T>>> producer_;

 public:
  explicit collector(std::vector<std::reference_wrapper<producer<T>>> producer)
      : producer<T>(), producer_(std::move(producer)) {
  }

  task<void> produce(unbuffered_single_chan<T>* tar_chan) override {
    COROBELT_LOG_INF_("collector starts production\n");

    if (producer_.size() < 2) {
      COROBELT_LOG_ERR_("must pass at least two producers\n");
      co_return;
    }

    std::priority_queue<std::pair<T, unbuffered_single_chan<T>*>,
                        std::vector<std::pair<T, unbuffered_single_chan<T>*>>,
                        comperator>
        sources_;
    std::vector<unbuffered_single_chan<T>*> channel_;
    std::vector<task<void>> tasks_;

    std::optional<T> msg;
    unbuffered_single_chan<T>* src_chan;
    for (auto it = producer_.begin(); it != producer_.end(); it++) {
      src_chan = new unbuffered_single_chan<T>{};
      if (!src_chan) {
        COROBELT_LOG_ERR_("initializing colelctor went wrong\n");
        co_return;
      }
      channel_.push_back(src_chan);
      producer<T>& prod = *it;
      task<void> producer_res = prod.produce(src_chan);
      tasks_.push_back(std::move(producer_res));
      msg = co_await src_chan->read();
      if (!msg) {
        COROBELT_LOG_ERR_("collector initial read went wrong\n");
        co_return;
      }
      sources_.push(std::make_pair(msg.value(), src_chan));
    }
    src_chan = nullptr;

    std::pair<T, unbuffered_single_chan<T>*> next_task;
    while (!sources_.empty()) {
      next_task = sources_.top();
      sources_.pop();
      src_chan = next_task.second;
      T m = next_task.first;

      if (!tar_chan || !co_await tar_chan->write(m)) {
        COROBELT_LOG_ERR_("writing within collector went wrong\n");
        co_return;
      }

      if (!src_chan || !src_chan->is_open()) {
        COROBELT_LOG_ERR_("null or closed, one producer finished\n");
        src_chan = nullptr;
        continue;
      }

      msg = co_await src_chan->read();
      if (msg) {
        sources_.push(std::make_pair(msg.value(), src_chan));
      } else {
        src_chan->close_chan();
      }
      src_chan = nullptr;
    }

    for (unbuffered_single_chan<T>* chan : channel_) {
      if (chan) {
        delete chan;
      }
    }

    for (auto& t : tasks_) {
      t.destroy();
    }

    COROBELT_LOG_INF_("leaf collector\n");
    co_return;
  }
};

template <typename T>
struct awaiter {
 private:
  producer<T>& producer_;
  consumer<T>& consumer_;

  struct do_nothing_consumer : public consumer<int> {
    task<void> consume(unbuffered_single_chan<T>* src_chan) override {
      std::optional<T> msg;
      while (co_await src_chan->read()) {
        COROBELT_LOG_INF_("received an element\n");
      }
    }
  };

  explicit awaiter(producer<T>& producer,
                   std::reference_wrapper<consumer<T>> consumer)
      : producer_(producer), consumer_(consumer) {
  }

  bool await_termination() {
    unbuffered_single_chan<T>* target_chan_ = new unbuffered_single_chan<T>{};

    if (!target_chan_) {
      return false;
    }

    //bool is_prod_only = std::dynamic_cast<collector<T> *>(&producer_):
    //is_prod_only = std::dynamic_cast<pipeline<T> *>(&producer_);

    task<void> consumer_task = consumer_.consume(target_chan_);
    task<void> producer_task = producer_.produce(target_chan_);

    while (producer_task.is_not_done()) {
      producer_task.resume_handle();
      //if (is_prod_only)
      //  consumer_task.resume_handle();
    }

    producer_task.destroy();
    consumer_task.destroy();

    if (target_chan_)
      delete target_chan_;

    return true;
  }

 public:
  static bool await_termination(producer<T>& producer,
                                std::reference_wrapper<consumer<T>> consumer) {
    awaiter<T> awaiter{producer, consumer};
    return awaiter.await_termination();
  }

  static bool await_termination(producer<T>& producer) {
    do_nothing_consumer consumer;
    return awaiter<T>::await_termination(producer, consumer);
  }
};

};  // namespace coroutine

}; // namespace sim

#endif // SIM_COROUTINE_H_
