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

#include <boost/coroutine2/all.hpp>
#include <memory>
#include <set>
#include <vector>

#include "lib/utils/log.h"

namespace corobelt {

#define COROBELT_DEBUG_ 1

// helper makro for erro print logging
#ifdef COROBELT_DEBUG_

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

  virtual void produce(coro_push_t<T> &sink) {
    return;
  };
};

template <typename T>
class Consumer {
 public:
  explicit Consumer() = default;

  virtual void consume(coro_pull_t<T> &source) {
    for ([[maybe_unused]] volatile T event : source) {
      ;
    }
    return;
  };
};

template <typename T>
class Pipe {
 public:
  explicit Pipe() = default;

  virtual void process(coro_push_t<T> &sink, coro_pull_t<T> &source) {
    for (T event : source) {
      sink(event);
    }
    return;
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

 protected:
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
      COROBELT_LOG_WARN_("Collector: no producers given\n");
      return;
    }

    // create producer coroutines
    for (corobelt::Producer<T> *prod : producer_) {
      corobelt::coro_pull_t<T> *src = new corobelt::coro_pull_t<T>(std::bind(
          &corobelt::Producer<T>::produce, prod, std::placeholders::_1));
      if (!move(src)) {
        COROBELT_LOG_WARN_(
            "Collector: a given coroutine does not create events\n");
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
        COROBELT_LOG_INF_("Collector: a coroutine finished producing\n");
      }
    }

    COROBELT_LOG_INF_("Collector: finished overall producing\n");
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
    //std::cout << "awaiter producer: " << producer_ << std::endl;

    if (producer_ == nullptr) {
      COROBELT_LOG_ERR_("Awaiter: no producer given\n");
      return false;
    }

    //std::cout << "try creating producer a.k.a parser" << std::endl;

    // create producer
    corobelt::coro_pull_t<T> producer(std::bind(
        &corobelt::Producer<T>::produce, producer_, std::placeholders::_1));
    if (!producer) {
      COROBELT_LOG_ERR_("Awaiter: could not create the producer\n");
      return false;
    }

    //std::cout << "created producer a.k.a parser" << std::endl;

    corobelt::coro_push_t<T> *consumer_ptr = nullptr;

    // create potentielly consumer
    if (consumer_ != nullptr) {
      consumer_ptr = new coro_push_t<T>(std::bind(
          &corobelt::Consumer<T>::consume, consumer_, std::placeholders::_1));
      if (is_finished<T>(consumer_ptr)) {
        if (consumer_ptr != nullptr) {
          delete consumer_ptr;
        }
        COROBELT_LOG_ERR_("Awaiter: could not create consumer\n");
        return false;
      }
    }

    for (T event : producer) {
      if (consumer_ptr != nullptr) {
        (*consumer_ptr)(event);
      }
    }

    COROBELT_LOG_INF_("Awaiter: finished awaiting\n");
    return true;
  }
};

template <typename T>
class Pipeline : public Producer<T> {
  Producer<T> *source_;
  std::vector<Pipe<T> *> pipes_;
  std::set<coro_pull_t<T> *> landfill_;

 protected:
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
      corobelt::coro_pull_t<T> *src = *it;
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
      COROBELT_LOG_ERR_("Pipeline: no source given\n");
      return;
    }

    coro_pull_t<T> *provider_ptr;

    provider_ptr = new coro_pull_t<T>(
        std::bind(&Producer<T>::produce, source_, std::placeholders::_1));
    if (!deposite(provider_ptr)) {
      if (provider_ptr != nullptr) {
        delete provider_ptr;
      }
      COROBELT_LOG_ERR_("Pipeline: could not create source producer\n");
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
        COROBELT_LOG_ERR_("Pipeline: could not create pipe\n");
        return;
      }
    }

    for (T event : *provider_ptr) {
      sink(event);
    }

    clearLandfill();
    COROBELT_LOG_INF_("Collector: finished overall producing\n");
    return;
  }
};

}  // namespace corobelt

#endif  // SIMBRICKS_TRACE_COROBELT_H_
