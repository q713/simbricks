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

#include <exception>
#include <iostream>
#include <memory>

#include "sync/channel.h"
#include "util/exception.h"
#include "util/factory.h"
#include "util/utils.h"

template<typename ValueType>
struct producer {
  explicit producer() = default;

  virtual concurrencpp::result<void> produce(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<ValueType>> tar_chan) {
    co_return;
  };
};

template<typename ValueType>
struct consumer {
  explicit consumer() = default;

  virtual concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<ValueType>> src_chan) {
    co_return;
  };
};

template<typename ValueType>
struct cpipe {
  explicit cpipe() = default;

  virtual concurrencpp::result<void> process(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<CoroChannel<ValueType>> src_chan,
      std::shared_ptr<CoroChannel<ValueType>> tar_chan) {
    co_return;
  }
};

/* Wrapper class to allow passing multiple pipelines to method */
template<typename ValueType>
struct pipeline {
  std::shared_ptr<producer<ValueType>> prod_;
  std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes_;
  std::shared_ptr<consumer<ValueType>> cons_;

  explicit pipeline(std::shared_ptr<producer<ValueType>> prod,
                    std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes,
                    std::shared_ptr<consumer<ValueType>> cons)
      : prod_(prod), pipes_(pipes), cons_(cons) {
  }
};

template<typename ValueType>
inline concurrencpp::result<void> run_pipeline_impl(
    std::shared_ptr<concurrencpp::executor> executor,
    pipeline<ValueType> &pipeline) {
  throw_if_empty(executor, resume_executor_null);

  const size_t amount_channels = pipeline.pipes_.size() + 1;
  std::vector<std::shared_ptr<CoroChannel<ValueType>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};
  //std::vector<concurrencpp::result<concurrencpp::result<void>>> tasks{amount_channels + 1};
  channels[0] = create_shared<CoroBoundedChannel<ValueType>>(channel_is_null);
  throw_if_empty(pipeline.prod_, producer_is_null);
  //tasks[0] = executor->submit([&](std::shared_ptr<Channel<ValueType>> tar) {
  //  return pipeline.prod_->produce(executor, tar);
  //}, channels[0]);
  tasks[0] = pipeline.prod_->produce(executor, channels[0]);

  for (size_t index = 0; index < pipeline.pipes_.size(); index++) {
    auto &pipe = pipeline.pipes_[index];
    throw_if_empty(pipe, pipe_is_null);

    channels[index + 1] =
        create_shared<CoroBoundedChannel<ValueType>>(channel_is_null);

    //tasks[index + 1] = executor->submit([&](std::shared_ptr<Channel<ValueType>> src,
    //                                        std::shared_ptr<Channel<ValueType>> tar) {
    //  return pipe->process(executor, src, tar);
    //}, channels[index], channels[index + 1]);
    tasks[index + 1] =
        pipe->process(executor, channels[index], channels[index + 1]);
  }
  throw_if_empty(pipeline.cons_, consumer_is_null);
  //tasks[amount_channels] = executor->submit([&](std::shared_ptr<Channel<ValueType>> src) {
  //  return pipeline.cons_->consume(executor, src);
  //}, channels[amount_channels - 1]);
  tasks[amount_channels] =
      pipeline.cons_->consume(executor, channels[amount_channels - 1]);

  for (size_t index = 0; index < amount_channels; index++) {
    //co_await co_await tasks[index];
    co_await tasks[index];
    co_await channels[index]->CloseChannel(executor);
  }
  co_await tasks[amount_channels];
  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> run_pipeline_impl_parallel(
    concurrencpp::executor_tag,
    std::shared_ptr<concurrencpp::executor> executor,
    pipeline<ValueType> &pipeline) {
  co_await run_pipeline_impl(executor, pipeline);
  co_return;
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> executor,
                         pipeline<ValueType> &pipeline) {
  try {
    run_pipeline_impl(executor, pipeline).get();
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> executor,
                         std::shared_ptr<producer<ValueType>> prod,
                         std::vector<std::shared_ptr<cpipe<ValueType>>> &pipes,
                         std::shared_ptr<consumer<ValueType>> cons) {
  pipeline<ValueType> pipeline{prod, pipes, cons};

  try {
    run_pipeline_impl(executor, pipeline).get();
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }
}

template<typename ValueType>
inline void run_pipeline(std::shared_ptr<concurrencpp::executor> executor,
                         std::shared_ptr<producer<ValueType>> prod,
                         std::shared_ptr<consumer<ValueType>> cons) {
  std::vector<std::shared_ptr<cpipe<ValueType>>> dummy;
  run_pipeline<ValueType>(executor, prod, dummy, cons);
}

template<typename ValueType>
inline void run_pipelines(std::shared_ptr<concurrencpp::executor> executor,
                          std::vector<pipeline<ValueType>> &pipelines) {
  size_t amount_tasks = pipelines.size();
  std::vector<concurrencpp::result<void>> pipelns{amount_tasks};

  try {
    // create asynchronous(NOTE: asynchronous does not equal parallel)
    // coroutines
    for (size_t index = 0; index < amount_tasks; index++) {
      auto &pipel = pipelines[index];
      pipelns[index] = run_pipeline_impl(executor, pipel);
    }
    // suspend to get result
    for (size_t index = 0; index < amount_tasks; index++) {
      pipelns[index].get();
    }
  } catch (std::exception &exe) {
    std::cerr << exe.what() << std::endl;
  }
}

template<typename ValueType>
inline void run_pipelines_parallel(
    std::shared_ptr<concurrencpp::executor> executor,
    std::vector<pipeline<ValueType>> &pipelines) {
  throw_if_empty(executor, resume_executor_null);
  size_t amount_tasks = pipelines.size();
  //std::vector<concurrencpp::result<concurrencpp::result<void>>> pipelns{
  //    amount_tasks};
  std::vector<concurrencpp::result<void>> pipelns{amount_tasks};

  try {
    // create asynchronous(NOTE: asynchronous does not equal parallel) coroutines
    for (size_t index = 0; index < amount_tasks; index++) {
      //pipelns[index] = executor->submit(run_pipeline_impl<ValueType>, executor,
      //                                  pipelines[index]);
      pipelns[index] = run_pipeline_impl_parallel({}, executor, pipelines[index]);
    }
    // suspend to get result
    for (size_t index = 0; index < amount_tasks; index++) {
      pipelns[index].get();
      std::cout << "one pipeline finished" << '\n';
    }
  } catch (std::exception &exe) {
    std::cerr << exe.what() << '\n';
  }

  std::cout << "all pipelines finished" << '\n';
}

inline void await_results(std::vector<concurrencpp::result<void>> &results) {
  for (auto &res : results) {
    res.get();
  }
}

#endif  // SIMBRICKS_TRACE_COROBELT_H_
