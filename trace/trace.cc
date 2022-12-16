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

#include <boost/coroutine2/all.hpp>
#include <memory>
#include <vector>

#include "lib/utils/cxxopts.hpp"
#include "lib/utils/log.h"
#include "set"
#include "trace/filter/symtable.h"
#include "trace/parser/parser.h"
#include "trace/reader/reader.h"

template <typename T>
using coro_pull_t = typename boost::coroutines2::coroutine<T>::pull_type;

template <typename T>
using coro_push_t = typename boost::coroutines2::coroutine<T>::push_type;

template <typename T>
bool is_finished(coro_pull_t<T> *src) {
  if (src && *src) {
    return false;
  }
  return true;
}

template <typename T>
bool is_finished(coro_push_t<T> *src) {
  if (src && *src) {
    return false;
  }
  return true;
}

template <typename T>
bool is_not_finished(coro_pull_t<T> *src) {
  return !is_finished<T>(src);
}

template <typename T>
class Producer {
 public:
  explicit Producer() = default;

  virtual void produce(coro_push_t<T> &sink){};
};

template <typename T>
class Consumer {
 public:
  explicit Consumer() = default;

  virtual void consume(coro_pull_t<T> &source){};
};

template <typename T>
class Pipe {
 public:
  explicit Pipe() = default;

  virtual void process(coro_push_t<T> &sink, coro_pull_t<T> &source) {
  }
};

template <typename T>
using col_key_t = typename std::pair<T, coro_pull_t<T> *>;

template <typename T, typename Compare = std::less<T>>
class Collector : public Producer<T> {
  struct Comperator {
   private:
    Compare compare_;

   public:
    inline bool operator()(const col_key_t<T> &le,
                           const col_key_t<T> &re) const {
      return compare_(le.first, re.first);
    }
  };

  // NOTE: producers have to be cleaned up by the caller
  std::vector<Producer<T> *> producer_;
  // NOTE: data within the sources set is clened up by this class
  std::set<col_key_t<T>, Comperator> sources_;

  col_key_t<T> pop() {
    auto e_src_pair_it = sources_.begin();
    col_key_t<T> e_src_pair(*e_src_pair_it);
    sources_.erase(e_src_pair_it);
    return e_src_pair;
  }

  bool move(coro_pull_t<T> *src) {
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

 public:
  Collector(std::vector<Producer<T> *> producer)
      : Producer<T>(), producer_{std::move(producer)} {
  }

  ~Collector() {
  }

  void produce(coro_push_t<T> &sink) override {
    if (producer_.empty()) {
      // std::cout << "no producers given" << std::endl;
      return;
    }
    // TODO: ERROR HANDLING AND CHECKS!!!!!!!!!!!!!!

    // create producer coroutines
    for (Producer<T> *prod : producer_) {
      coro_pull_t<T> *src = new coro_pull_t<T>(
          std::bind(&Producer<T>::produce, prod, std::placeholders::_1));
      if (!move(src)) {
        // std::cout << "coroutines doesnt create events" << std::endl;
      }
    }

    // process events in order
    while (!sources_.empty()) {
      col_key_t<T> e_src_pair = std::move(pop());

      // pass event further
      T event = e_src_pair.first;
      sink(event);

      // if coroutine insert next
      coro_pull_t<T> *src = e_src_pair.second;
      if (!move(src)) {
        // TODO: debug print for coroutine termination info
        // std::cout << "finished a coroutine" << std::endl;
      }
    }

    return;
  }
};

template <typename T>
class Awaiter {
  Producer<T> *producer_ = nullptr;
  Consumer<T> *consumer_ = nullptr;

 public:
  explicit Awaiter(Producer<T> *producer, Consumer<T> *consumer)
      : producer_(producer), consumer_(consumer) {
  }

  explicit Awaiter(Producer<T> *producer) : producer_(producer) {
  }

  bool await_termination() {
    if (producer_ == nullptr) {
      // TODO: print error
      return false;
    }

    // create producer
    coro_pull_t<T> producer(
        std::bind(&Producer<T>::produce, producer_, std::placeholders::_1));
    if (!producer) {
      // TODO: print error
      return false;
    }

    coro_push_t<T> *consumer_ptr = nullptr;

    // create potentielly consumer
    if (consumer_ != nullptr) {
      consumer_ptr = new coro_push_t<T>(
          std::bind(&Consumer<T>::consume, consumer_, std::placeholders::_1));
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
};

template <typename T>
class Pipeline : public Producer<T> {
  Producer<T> *source_;
  std::vector<Pipe<T> *> pipes_;
  std::set<coro_pull_t<T> *> landfill_;

  bool deposite(coro_pull_t<T> *to_dump) {
    if (is_finished<T>(to_dump)) {
      return false;
    }
    auto res = landfill_.insert(to_dump);
    if (!res.second) {
      return false;
    }
    return true;
  }

  void clearLandfill() {
    for (auto it = landfill_.begin(); it != landfill_.end(); it++) {
      coro_pull_t<T> *src = *it;
      if (src != nullptr) {
        delete src;
      }
    }
    landfill_.clear();
  }

 public:
  Pipeline(Producer<T> *source, std::vector<Pipe<T> *> pipes)
      : source_(source), pipes_(pipes) {
  }

  Pipeline(Producer<T> *source) : source_(source) {
  }

  ~Pipeline() {
  }

  void produce(coro_push_t<T> &sink) override {
    if (source_ == nullptr) {
      // TODO: error print
      return;
    }

    std::set<coro_pull_t<T> *> account;
    coro_pull_t<T> *provider_ptr;

    provider_ptr = new coro_pull_t<T>(
        std::bind(&Producer<T>::produce, source_, std::placeholders::_1));
    if (!deposite(provider_ptr)) {
      if (provider_ptr != nullptr) {
        delete provider_ptr;
      }
      // TODO: error print
      return;
    }

    for (Pipe<T> *pipe : pipes_) {
      provider_ptr = new coro_pull_t<T>(std::bind(&Pipe<T>::process, pipe,
                                                  std::placeholders::_1,
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
};

class IntProd : public Producer<int> {
  int id_;

 public:
  IntProd(int id) : Producer<int>(), id_(id) {
  }

  void produce(coro_push_t<int> &sink) override {
    for (int e = 0; e < 10; e++) {
      sink(id_);
    }
    return;
  }
};

class IntAdder : public Pipe<int> {
 public:
  IntAdder() : Pipe<int>() {
  }

  void process(coro_push_t<int> &sink, coro_pull_t<int> &source) override {
    for (int event : source) {
      event += 10;
      sink(event);
    }
    return;
  }
};

class IntPrinter : public Consumer<int> {
 public:
  explicit IntPrinter() : Consumer<int>() {
  }

  void consume(coro_pull_t<int> &source) override {
    std::cout << "start to sinkhole events" << std::endl;
    for (int e : source) {
      std::cout << "consumed " << e << std::endl;
    }
    return;
  }
};

int main(int argc, char *argv[]) {
  IntProd gen1(1);
  IntProd gen2(2);
  IntAdder adder1;
  IntAdder adder2;
  IntAdder adder3;
  IntPrinter printer;
  Pipeline<int> pipeline1(&gen1, {&adder1});
  Pipeline<int> pipeline2(&gen2, {&adder2});
  Collector<int> collector({&pipeline1, &pipeline2});
  Pipeline<int> pipeline3(&collector, {&adder3});
  Awaiter<int> awaiter(&pipeline3, &printer);

  if (!awaiter.await_termination()) {
    std::cout << "could not await termination of the pipeline" << std::endl;
  }

  // coro_pull_t<int> ints(
  //     std::bind(&Producer<int>::produce, &collector, std::placeholders::_1));
  // std::cout << "start generation" << std::endl;
  // for (auto i : ints) {
  //   std::cout << "generated: " << i << std::endl;
  // }
  // std::cout << "everything was generated" << std::endl;

  /*
  std::string linux_dump;
  std::string gem5_log;
  std::string nicbm_log;

  cxxopts::Options options("Tracing", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")(
      "linux-dump",
      "file path to a output file obtained by 'objdump --syms linux_image'",
      cxxopts::value<std::string>(linux_dump))(
      "gem5-log", "file path to a log file written by gem5",
      cxxopts::value<std::string>(gem5_log))(
      "nicbm-log", "file path to a log file written by the nicbm",
      cxxopts::value<std::string>(nicbm_log));

  try {
    cxxopts::ParseResult result = options.parse(argc, argv);

    if (result.count("help")) {
      printf("%s", options.help().c_str());
      exit(EXIT_SUCCESS);
    }

    if (!result.count("linux-dump")) {
      DLOGERR("could not parse option 'linux-dump'\n");
      exit(EXIT_FAILURE);
    }

    if (!result.count("gem5-log")) {
      DLOGERR("could not parse option 'gem-5-log'\n");
      exit(EXIT_FAILURE);
    }

    //if (!result.count("nicbm-log")) {
    //  DLOGERR("could not parse option 'nicbm-log'\n");
    //  exit(EXIT_FAILURE);
    //}

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  LineReader ssymsLr;
  SymsSyms syms_filter{"SymbolTableFilter", ssymsLr};
  //syms_filter("entry_SYSCALL_64")
  //  ("__do_sys_gettimeofday")
  //  ("__sys_sendto")
  //  ("i40e_lan_xmit_frame")
  //  ("syscall_return_via_sysret")
  //  ("__sys_recvfrom")
  //  ("deactivate_task")
  //  ("interrupt_entry")
  //  ("i40e_msix_clean_rings")
  //  ("napi_schedule_prep")
  //  ("__do_softirq")
  //  ("trace_napi_poll")
  //  ("net_rx_action")
  //  ("i40e_napi_poll")
  //  ("activate_task")
  //  ("copyout");
  //SSyms syms_filter("SymbolTableFilter", ssymsLr);
  //syms_filter("entry_SYSCALL_64")
  //  ("__do_sys_gettimeofday")
  //  ("__sys_sendto")
  //  ("i40e_lan_xmit_frame")
  //  ("syscall_return_via_sysret")
  //  ("__sys_recvfrom")
  //  ("deactivate_task")
  //  ("interrupt_entry")
  //  ("i40e_msix_clean_rings")
  //  ("napi_schedule_prep")
  //  ("__do_softirq")
  //  ("trace_napi_poll")
  //  ("net_rx_action")
  //  ("i40e_napi_poll")
  //  ("activate_task")
  //  ("copyout");

  if (!syms_filter.load_file(linux_dump)) {
    DFLOGERR("could not load file with path '%s'\n", linux_dump);
    exit(EXIT_FAILURE);
  }

  ComponentFilter compF("Component Filter");
  compF("system.switch_cpus")("system.cpu");

  LineReader gem5Lr;
  Gem5Parser gem5Par{"Gem5Parser", syms_filter, compF, gem5Lr};
  if (!gem5Par.parse(gem5_log)) {
    DFLOGERR("could not parse gem5 log file with path '%s'\n",
             linux_dump.c_str());
    exit(EXIT_FAILURE);
  }

  //LineReader lr;
  //NicBmParser nicBmPar("NicBmParser", lr);
  //if (!nicBmPar.parse(nicbm_log)) {
  //  DFLOGERR("could not parse nicbm log file with path '%s'\n",
  nicbm_log.c_str());
  //  exit(EXIT_FAILURE);
  //}

  // TODO:
  // 1) check for parsing 'objdump -S vmlinux'
  // 2) which gem5 flags -> before witing parser --> use Exec without automatic
  // translation + Syscall
  // 3) gem5 parser -> gem5 events!!!
  // 4) nicbm parser
  // 5) handle symbricks events in all parsers
  // 6) merge events by timestamp
  // 7) how should events look like?
  // 8) add identifiers to know sources
  // 9) try trace?
  // 10) default events for not parsed lines / unexpected lines?
  // 11) gem5 logfile only from checkpoint on
  // 12) gem5 only write logfile only after certain timestamp

  */

  exit(EXIT_SUCCESS);
}