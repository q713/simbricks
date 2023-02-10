
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

namespace experimental_coroutine {

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

  task<value_type> get_return_object() noexcept {
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
      //std::cout << "resume the handle" << std::endl;
      handle_.resume();
    }
  }

  void destroy() {
    //std::cout << "start destruction of task_base" << std::endl;
    destroyed_ = true;
    if (handle_) {
      //std::cout << "task base destroy handle" << std::endl;
      handle_.destroy();
    }
  }

  explicit task_base(std::coroutine_handle<promise_type> handle)
      : handle_(handle) {
        //std::cout << "base task created" << std::endl;
  }

  ~task_base() {
    //std::cout << "delete task_base" << std::endl;
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

task<void> void_task_promise::get_return_object() noexcept {
  return task<void>{
      std::coroutine_handle<void_task_promise>::from_promise(*this)};
};

struct chan_void_task {
  // forward declaration + needed compiler hint
  struct chan_void_promise;
  using promise_type = chan_void_promise;

  struct chan_void_promise {
    chan_void_task get_return_object() noexcept {
      return {};
    };

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
};

// TODO: adapt for multithreading + buffered version + multi consumer/producer
template <typename T>
struct unbuffered_single_chan {
  struct chan_reader {
    unbuffered_single_chan<T>* chan_;

    bool await_ready() const noexcept {
      // std::cout << "enter await_ready chan_reader" << std::endl;
      if (!chan_->has_val_ || !chan_->writer_resumable_) {
        return false;
      }
      std::swap(chan_->reader_resumable_, chan_->writer_resumable_);
      // std::cout << "exit await_ready chan_reader" << std::endl;
      return true;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<void> coro) const noexcept {
      // std::cout << "enter await_suspend chan_reader" << std::endl;
      if (!chan_) {
        //std ::cout << "await_suspend chan_reader channel is gone" << std::endl;
        return std::noop_coroutine();
      }
      chan_->reader_resumable_ = coro;
      if (!chan_->has_val_ && chan_->writer_resumable_) {
        // std::cout << "special path await_suspend chan_reader" << std::endl;
        auto resumable = chan_->writer_resumable_;
        chan_->writer_resumable_ = nullptr;
        if (resumable && !resumable.done()) {
          //resumable.resume();
          return resumable;
        }
        if (chan_ && chan_->reader_resumable_ &&
            !chan_->reader_resumable_.done()) {
          //chan_->reader_resumable_.resume();
          return chan_->reader_resumable_;
        }
      }
      return std::noop_coroutine();
      // std::cout << "exit await_suspend chan_reader" << std::endl;
    }

    std::optional<T> await_resume() const noexcept {
      // std::cout << "enter await_resume chan_reader" << std::endl;
      if (!chan_) {
        //std::cout << "exit await_resume channel is null" << std::endl;
        return std::nullopt;
      }

      if (chan_->is_closed_) {
        //std::cout << "exit await_resume chan_reader closed channel"
        //          << std::endl;
        return std::nullopt;
      }

      if (!chan_->has_val_) {
        //std::cout << "await resume, but no value in channel" << std::endl;
        return std::nullopt;
      }
      chan_->has_val_ = false;
      // std::cout << "exit await_resume chan_reader" << std::endl;
      return std::optional<T>{chan_->value_};
    }

    explicit chan_reader(unbuffered_single_chan<T>* chan) : chan_(chan) {
    }
  };

  struct chan_writer {
    T value_to_write_;
    unbuffered_single_chan<T>* chan_;

    bool await_ready() const noexcept {
      // std::cout << "enter await_ready chan_writer" << std::endl;
      if (chan_->has_val_ || !chan_->reader_resumable_) {
        return false;
      }
      std::swap(chan_->reader_resumable_, chan_->writer_resumable_);
      // std::cout << "exit await_ready chan_writer" << std::endl;
      return true;
    }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<void> coro) const noexcept {
      // std::cout << "enter await_suspend chan_writer" << std::endl;
      if (!chan_) {
        //std::cout << "await_suspend chan_writer channel is null" << std::endl;
        return std::noop_coroutine();
      }
      chan_->writer_resumable_ = coro;
      return std::noop_coroutine();
      // std::cout << "exit await_suspend chan_writer" << std::endl;
    }

    bool await_resume() const noexcept {
      // std::cout << "enter await_resume chan_writer" << std::endl;
      if (!chan_) {
        //std::cout << "await_resume chan_writer channel is gone" << std::endl;
        return false;
      }

      if (chan_->is_closed_) {
        //std::cout << "await_resume chan_writer channel is closed" << std::endl;
        return false;
      }

      // std::cout << "try setting value chan_writer" << std::endl;
      chan_->has_val_ = true;
      chan_->value_ = value_to_write_;
      // std::cout << "setted value chan_writer" << std::endl;
      if (chan_->writer_resumable_ && !chan_->is_closed_) {
        // std::cout << "special path writer" << std::endl;
        auto resumable = chan_->writer_resumable_;
        chan_->writer_resumable_ = nullptr;
        if (resumable && !resumable.done()) {
          resumable.resume();
        }
      }
      // std::cout << "exit await_resume chan_writer" << std::endl;
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
    //std::cout << "start channel destruction" << std::endl;
    is_closed_ = true;
    //std::cout << "finished channel destruction" << std::endl;
  }
};

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
    //std::cout << "start destruction of pipeline" << std::endl;
    for (task<void>& ta : tasks_) {
      ta.destroy();
    }

    for (unbuffered_single_chan<T>* chan : channels_) {
      if (chan) {
        delete chan;
      }
    }
    //std::cout << "finished destruction of pipeline" << std::endl;
  }

  task<void> produce(unbuffered_single_chan<T>* tar_chan) override {
    //std::cout << "start pipeline" << std::endl;
/*
    // TODO: null checks
    std::optional<T> msg;
    using queue_t = std::pair<unbuffered_single_chan<T>*, unbuffered_single_chan<T>*>;
    std::vector<std::pair<unbuffered_single_chan<T>*, unbuffered_single_chan<T>*>> production_line;
    unbuffered_single_chan<T> *src_chan= new unbuffered_single_chan<T>{};
    unbuffered_single_chan<T> *intermediate_tar_chan;
    if (!src_chan) {
      std::cout << "fata initial malloc error" << std::endl;
      co_return;
    }
    // init producer
    task<void> producer_task = producer_.produce(src_chan);
    tasks_.push_back(producer_task);
    production_line.push_back(std::make_pair(src_chan, nullptr));

    // init pipes
    for (auto it = pipes_.begin(); it != pipes_.end(); it++) {
      src_chan = new unbuffered_single_chan<T>{};
      intermediate_tar_chan = new unbuffered_single_chan<T>{};
      if (!src_chan || !intermediate_tar_chan) {
        std::cout << "initalizing malloc error" << std::endl;
      }

      pipe<T>& p = *it;
      task<void> t = p.process(src_chan, intermediate_tar_chan);
      tasks_.push_back(t);
      production_line.push_back(std::make_pair(src_chan, intermediate_tar_chan));
    }

    while(producer_task.is_not_done()) {
      std::cout << "iteradte pipeline" << std::endl;
      for (size_t i=0; i<production_line.size(); i++) {
        auto it = production_line.at(i);
        src_chan = it.first;
        intermediate_tar_chan = it.second;

        if (src_chan && !intermediate_tar_chan) { // producer channel
          std::cout << "producer habdleing" << std::endl;
          if (src_chan->is_closed()) {
            std::cout << "producer channel is already closed" << std::endl;
            continue;
          }

          std::cout << "try reading produced value" << std::endl;
          msg = co_await src_chan->read();
          std::cout << "got message from producer" << std::endl;
          if (!msg) {
            std::cout << "producer message is invalid" << std::endl;
            //src_chan->close_chan();
          }

          std::cout << "executed initial read from producer" << std::endl;

          continue;
        } else if (src_chan && intermediate_tar_chan) { // pipe channel
          std::cout << "pipe habdleing" << std::endl;
          if (!msg) {
            std::cout << "message invalid for intermediate pipe" << std::endl;
            continue;
          }

          if (src_chan->is_closed() || intermediate_tar_chan->is_closed()) {
            std::cout << "pipe channel is already closed" << std::endl;
            continue;
          }

          if (!co_await src_chan->write(msg.value())) {
            std::cout << "writing to pipe input failed" << std::endl;
            continue;
          }
          
          msg = co_await intermediate_tar_chan->read();
          if (!msg) {
            std::cout << "could not receive a value from a pipe" << std::endl;
          }
          std::cout << "executed pipeing to pipe" << std::endl;
          continue;
        }
      }
      
      if (!msg) {
        std::cout << "invalid message for final write" << std::endl;
        continue;
      }
      if (!co_await tar_chan->write(msg.value())) {
        std::cout << "final write failed" << std::endl;
      }
      //producer_task.resume_handle();
    }
*/
    



    unbuffered_single_chan<T>* src_chan;
    for (auto it = pipes_.crbegin(); it != pipes_.crend(); it++) {
      src_chan = new unbuffered_single_chan<T>{};
      if (!src_chan) {
        //std::cout << "pipeline initialization error" << std::endl;
        co_return;
      }
      channels_.push_back(src_chan);
      pipe<T>& p = *it;
      task<void> t = p.process(src_chan, tar_chan);
      tasks_.push_back(t);
      tar_chan = src_chan;
    }

    //std::cout << "pipeline started all pipes" << std::endl;

    unbuffered_single_chan<T>* pipe_end = new unbuffered_single_chan<T>{};
    if (!pipe_end) {
      //std::cout << "pipeline allocation error" << std::endl;
      co_return;
    }
    channels_.push_back(pipe_end);
    task<void> t = producer_.produce(pipe_end);
    //task<void> t = producer_.produce(tar_chan);
    tasks_.push_back(t);

    std::optional<T> msg;
    
    do {
      //std::cout << "try to resume handle within pipeline" << std::endl;
      //for (auto &ta : tasks_) {
      //  ta.resume_handle();
      //}
      for (msg = co_await pipe_end->read(); msg; msg = co_await pipe_end->read()) {
        if (!tar_chan || !co_await tar_chan->write(msg.value())) {
          //std::cout << "pipeline write failure" << std::endl;
          break;
        }
        //std::cout << "pipeline forwarded value to receiver" << std::endl;
      }

      if (t.is_not_done())
        t.resume_handle();
    } while(t.is_not_done());

    //std::cout << "leave pipeline" << std::endl;
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
    //std::cout << "collector starts production" << std::endl;

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
        //std::cout << "initializing colelctor went wrong" << std::endl;
        co_return;
      }
      channel_.push_back(src_chan);
      producer<T>& prod = *it;
      task<void> producer_res = prod.produce(src_chan);
      tasks_.push_back(std::move(producer_res));
      //std::cout << "collector read initial value from producer" << std::endl;
      msg = co_await src_chan->read();
      if (!msg) {
        //std::cout << "collector initial read went wrong" << std::endl;
        co_return;
      }
      // sources_.insert(std::make_pair(msg.value(), src_chan));
      sources_.push(std::make_pair(msg.value(), src_chan));
      //std::cout << "added a new producer with channel" << std::endl;
    }
    src_chan = nullptr;

    //std::cout << "collector successfully initialized the producers"
    //          << std::endl;

    std::pair<T, unbuffered_single_chan<T>*> next_task;
    while (!sources_.empty()) {
      // auto next_task_it = sources_.begin();
      // next_task = *next_task_it;
      // sources_.erase(next_task_it);
      next_task = sources_.top();
      sources_.pop();
      src_chan = next_task.second;
      T m = next_task.first;

      //std::cout << "within collector try to write: " << m << std::endl;
      if (!tar_chan || !co_await tar_chan->write(m)) {
        //std::cout
        //    << "writing within collector went wrong!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        //    << std::endl;
        co_return;
      }

      if (!src_chan || !src_chan->is_open()) {
        //std::cout << "null or closed, one producer finished" << std::endl;
        src_chan = nullptr;
        continue;
      }

      msg = co_await src_chan->read();
      if (msg) {
        //std::cout << "within collector received: " << msg.value() <<
        //std::endl;
        /// sources_.insert(std::make_pair(msg.value(), src_chan));
        sources_.push(std::make_pair(msg.value(), src_chan));
      } else {
        //std::cout << "one producer finished" << std::endl;
        src_chan->close_chan();
      }
      //std::cout << "size of the queue: " << sources_.size() << std::endl;
      src_chan = nullptr;
    }

    //std::cout << "delete channels in collector" << std::endl;

    for (unbuffered_single_chan<T>* chan : channel_) {
      if (chan) {
        delete chan;
      }
    }

    for (auto& t : tasks_) {
      t.destroy();
    }

    //std::cout << "leaf collector" << std::endl;
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
        std::cout << "received an element" << std::endl;
      }
    }
  };

  explicit awaiter(producer<T>& producer,
                   std::reference_wrapper<consumer<T>> consumer)
      : producer_(producer), consumer_(consumer) {
  }

  ~awaiter() {
    //std::cout << "start destruction of awaiter" << std::endl;
  }

  bool await_termination() {
    unbuffered_single_chan<T>* target_chan_ = new unbuffered_single_chan<T>{};

    if (!target_chan_) {
      return false;
    }

    task<void> producer_task = producer_.produce(target_chan_);
    task<void> consumer_task = consumer_.consume(target_chan_);

    while (producer_task.is_not_done()/* || consumer_task.is_not_done()*/) {
      //if (producer_task.is_not_done())
      producer_task.resume_handle();

      //if (consumer_task.is_not_done())
      //  consumer_task.resume_handle();
      //std::cout << "execute loop in awaiter" << std::endl;
    }

    producer_task.destroy();
    consumer_task.destroy();

    //std::cout << "leafe awaiter" << std::endl;
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

class IntProducer : public producer<int> {
  int start_ = 0;

 public:
  IntProducer(int start) : producer<int>(), start_(start) {
  }

  virtual task<void> produce(unbuffered_single_chan<int>* tar_chan) override {
    for (int i = 0; i < 1'000'000 && tar_chan && tar_chan->is_open(); i++) {
      int msg = start_ + i;
      if (!tar_chan || !co_await tar_chan->write(msg)) {
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

class IntConsumer : public consumer<int> {
 public:
  explicit IntConsumer() : consumer<int>() {
  }

  task<void> consume(unbuffered_single_chan<int>* src_chan) override {
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

class IntAdder : public pipe<int> {
 public:
  task<void> process(unbuffered_single_chan<int>* src_chan,
                     unbuffered_single_chan<int>* tar_chan) override {
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

/*
generator<int> counter() {
  for (int i = 0; i < 10; i++) {
    co_yield i;
  }
}
*/

task<bool> produce(int start, unbuffered_single_chan<int>* tar_chan) {
  for (int i = 0; i < 10; i++) {
    int m = i + start;
    if (!(co_await tar_chan->write(m))) {
      std::cout << "write failure, start=" << start << std::endl;
      co_return false;
    }
    std::cout << "wrote value to channel, start=" << start << std::endl;
  }
  std::cout << "exited producer, start=" << start << std::endl;
  co_return true;
}

task<bool> consume() {
  struct comp {
   public:
    inline bool operator()(
        const std::pair<int, unbuffered_single_chan<int>*>& le,
        const std::pair<int, unbuffered_single_chan<int>*>& re) const {
      return le.first > re.first;
    }
  };
  std::priority_queue<std::pair<int, unbuffered_single_chan<int>*>,
                      std::vector<std::pair<int, unbuffered_single_chan<int>*>>,
                      comp>
      queue;

  auto src_chanA = new experimental_coroutine::unbuffered_single_chan<int>();
  auto src_chanB = new experimental_coroutine::unbuffered_single_chan<int>();
  auto src_chanC = new experimental_coroutine::unbuffered_single_chan<int>();
  experimental_coroutine::task<bool> producer_resA =
      experimental_coroutine::produce(0, src_chanA);
  experimental_coroutine::task<bool> producer_resB =
      experimental_coroutine::produce(1, src_chanB);
  experimental_coroutine::task<bool> producer_resC =
      experimental_coroutine::produce(2, src_chanC);

  std::optional<int> msg;
  msg = co_await src_chanA->read();
  if (!msg) {
    std::cout << "initial read failure" << std::endl;
    co_return false;
  }
  queue.push(std::make_pair(msg.value(), src_chanA));

  msg = co_await src_chanB->read();
  if (!msg) {
    std::cout << "initial read failure" << std::endl;
    co_return false;
  }
  queue.push(std::make_pair(msg.value(), src_chanB));

  msg = co_await src_chanC->read();
  if (!msg) {
    std::cout << "initial read failure" << std::endl;
    co_return false;
  }
  queue.push(std::make_pair(msg.value(), src_chanC));

  std::cout << "after initializitaion, queue has size=" << queue.size()
            << std::endl;

  unbuffered_single_chan<int>* chan;
  while (!queue.empty()) {
    auto pair = queue.top();
    queue.pop();
    int m = pair.first;
    chan = pair.second;
    // std::cout << "consumer fetched value" << std::endl;

    std::cout << "took value=" << m << " from channel" << std::endl;

    if (!chan) {
      std::cout << "found null channel" << std::endl;
      continue;
    }

    if (chan->is_closed()) {
      std::cout << "currently having a closed channel" << std::endl;
      continue;
    }

    // if (!co_await chan->write(msg.value())) {
    //   std::cout << "consumer write failure" << std::endl;
    //   co_return false;
    // }

    msg = co_await chan->read();
    if (msg) {
      queue.push(std::make_pair(msg.value(), chan));
    } else {
      std::cout << "closed a channel" << std::endl;
      chan->close_chan();
    }
  }

  std::cout << "start deleting channels in consumer" << std::endl;

  delete src_chanA;
  delete src_chanB;
  delete src_chanC;

  std::cout << "exited consumer" << std::endl;
  co_return true;
}


};  // namespace experimental_coroutine


int main(int argc, char* argv[]) {
#if 0
  auto target_chan = new experimental_coroutine::unbuffered_single_chan<int>();
  auto consumer_res = experimental_coroutine::consume();

  int producers = 3;
  while (consumer_res.is_not_done()) {
    consumer_res.resume_handle();
    std::cout << "executed loop" << std::endl;
  }

  std::cout << "consumer result: " << consumer_res.return_value() << std::endl;

  delete target_chan;
#else
  struct dummy : public experimental_coroutine::pipe<int> {
    experimental_coroutine::task<void> process(
        experimental_coroutine::unbuffered_single_chan<int>* src_chan,
        experimental_coroutine::unbuffered_single_chan<int>* tar_chan) {
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

  // experimental_coroutine::unbuffered_single_chan<int> chan;
  experimental_coroutine::IntProducer prodA{0};
  experimental_coroutine::IntProducer prodB{1};
  experimental_coroutine::IntProducer prodC{2};
  experimental_coroutine::IntProducer prodD{3};
  experimental_coroutine::IntConsumer cons;
  experimental_coroutine::IntAdder adder;
  dummy d;
  experimental_coroutine::collector<int> col{{prodA, prodB}};
  experimental_coroutine::pipeline<int> pip{col,
                                            {adder, d}};
  if (experimental_coroutine::awaiter<int>::await_termination(pip)) {
    std::cout << "awaiter finished successful" << std::endl;
  } else {
    std::cout << "awaiter finished with error!!" << std::endl;
  }
#endif

  return 0;
}