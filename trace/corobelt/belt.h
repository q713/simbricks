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

 protected:
  col_key_t<T> pop();

  bool move(coro_pull_t<T> *src);

 public:
  Collector(std::vector<Producer<T> *> producer)
      : Producer<T>(), producer_{std::move(producer)} {
  }

  ~Collector() {
  }

  void produce(coro_push_t<T> &sink) override;
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

  bool await_termination();
};

template <typename T>
class Pipeline : public Producer<T> {
  Producer<T> *source_;
  std::vector<Pipe<T> *> pipes_;
  std::set<coro_pull_t<T> *> landfill_;

 protected:
  bool deposite(coro_pull_t<T> *to_dump);

  void clearLandfill();

 public:
  Pipeline(Producer<T> *source, std::vector<Pipe<T> *> pipes)
      : source_(source), pipes_(pipes) {
  }

  Pipeline(Producer<T> *source) : source_(source) {
  }

  ~Pipeline() {
  }

  void produce(coro_push_t<T> &sink) override;
};

}  // namespace corobelt

#endif  // SIMBRICKS_TRACE_COROBELT_H_
