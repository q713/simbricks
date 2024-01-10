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
#include <algorithm>

#include "spdlog/spdlog.h"

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
    std::optional<ValueType> val_opt;
    for (val_opt = co_await src_chan->Pop(resume_executor);
         val_opt;
         val_opt = co_await src_chan->Pop(resume_executor));
    co_return;
  };
};

template<typename ValueType>
using NoOpConsumer = consumer<ValueType>;

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

// TODO: may make this a concurrencpp::generator<...>
template<typename ValueType>
class Producer {
 public:
  explicit Producer() = default;

  virtual explicit operator bool() noexcept {
    return false;
  };

  virtual concurrencpp::result<std::optional<ValueType>> produce(std::shared_ptr<concurrencpp::executor> executor) {
    return {};
  };
};

template<typename ValueType>
class Consumer {
 public:
  explicit Consumer() = default;

  virtual concurrencpp::result<void> consume(std::shared_ptr<concurrencpp::executor> executor, ValueType value) {
    co_return;
  };
};

template<typename ValueType>
class Handler {
 public:
  explicit Handler() = default;

  virtual concurrencpp::result<bool> handel(std::shared_ptr<concurrencpp::executor> executor, ValueType &value) {
    co_return false;
  };
};

template<typename ValueType>
class Pipeline {
 public:
  std::shared_ptr<Producer<ValueType>> prod_;
  std::shared_ptr<std::vector<std::shared_ptr<Handler<ValueType>>>> handler_;
  std::shared_ptr<Consumer<ValueType>> cons_;

  explicit Pipeline(const std::shared_ptr<Producer<ValueType>> &prod,
                    const std::shared_ptr<std::vector<std::shared_ptr<Handler<ValueType>>>> &handler,
                    const std::shared_ptr<Consumer<ValueType>> &cons)
      : prod_(prod), handler_(handler), cons_(cons) {
    throw_if_empty(prod_, TraceException::kProducerIsNull, source_loc::current());
    throw_if_empty(handler_, TraceException::kHandlerIsNull, source_loc::current());
    for (std::shared_ptr<Handler<ValueType>> &han : *handler_) {
      throw_if_empty(han, TraceException::kProducerIsNull, source_loc::current());
    }
    throw_if_empty(cons_, TraceException::kConsumerIsNull, source_loc::current());
  }
};

template<typename ValueType>
inline concurrencpp::result<void> Produce(concurrencpp::executor_tag,
                                          std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                                          std::shared_ptr<Producer<ValueType>> producer,
                                          std::shared_ptr<CoroChannel<ValueType>> tar_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(producer, TraceException::kProducerIsNull, source_loc::current());

  while (*producer) {
    std::optional<ValueType> value = co_await producer->produce(tpe);
    if (not value.has_value()) {
      break;
    }

    const bool could_push = co_await tar_chan->Push(tpe, *value);
    throw_on(not could_push,
             "unable to push next event to target channel",
             source_loc::current());
  }

  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> Consume(concurrencpp::executor_tag,
                                          std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                                          std::shared_ptr<Consumer<ValueType>> consumer,
                                          std::shared_ptr<CoroChannel<ValueType>> src_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(consumer, TraceException::kConsumerIsNull, source_loc::current());

  std::optional<ValueType> opt_val;
  for (opt_val = co_await src_chan->Pop(tpe); opt_val.has_value(); opt_val = co_await src_chan->Pop(tpe)) {
    ValueType value = *opt_val;

    co_await consumer->consume(tpe, value);
  }

  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> Handel(concurrencpp::executor_tag,
                                         std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                                         std::shared_ptr<Handler<ValueType>> handler,
                                         std::shared_ptr<CoroChannel<ValueType>> src_chan,
                                         std::shared_ptr<CoroChannel<ValueType>> tar_chan) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(src_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(tar_chan, TraceException::kChannelIsNull, source_loc::current());
  throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

  std::optional<ValueType> opt_val;
  for (opt_val = co_await src_chan->Pop(tpe); opt_val.has_value(); opt_val = co_await src_chan->Pop(tpe)) {
    ValueType value = *opt_val;

    const bool pass_on = co_await handler->handel(tpe, value);

    if (pass_on) {
      const bool could_push = co_await tar_chan->Push(tpe, value);
      throw_on(not could_push,
               "unable to push next event to target channel",
               source_loc::current());
    }
  }

  co_return;
}

template<typename ValueType>
inline concurrencpp::result<void> RunPipelineImpl(std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                                                  std::shared_ptr<Pipeline<ValueType>> pipeline) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

  const size_t amount_channels = pipeline->handler_->size() + 1;
  std::vector<std::shared_ptr<CoroChannel<ValueType>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};

  channels[0] = create_shared<CoroBoundedChannel<ValueType>>(TraceException::kChannelIsNull);
  throw_if_empty(pipeline->prod_, TraceException::kProducerIsNull, source_loc::current());
  tasks[0] = Produce({}, tpe, pipeline->prod_, channels[0]);

  for (int index = 0; index < pipeline->handler_->size(); index++) {
    auto &handl = *(pipeline->handler_);
    auto &handler = handl[index];
    throw_if_empty(handler, TraceException::kHandlerIsNull, source_loc::current());

    channels[index + 1] = create_shared<CoroBoundedChannel<ValueType>>(TraceException::kChannelIsNull);

    tasks[index + 1] = Handel({}, tpe, handler, channels[index], channels[index + 1]);
  }

  throw_if_empty(pipeline->cons_, TraceException::kConsumerIsNull, source_loc::current());
  tasks[amount_channels] = Consume({}, tpe, pipeline->cons_, channels[amount_channels - 1]);

  for (int index = 0; index < amount_channels; index++) {
    co_await tasks[index];
    co_await channels[index]->CloseChannel(tpe);
  }
  co_await tasks[amount_channels];

  co_return;
}

template<typename ValueType>
inline void RunPipeline(std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                        std::shared_ptr<Pipeline<ValueType>> pipeline) {
  spdlog::info("start a pipeline");
  RunPipelineImpl(tpe, pipeline).get();
  spdlog::info("finished a pipeline");
}

template<typename ValueType>
inline concurrencpp::result<void> RunPipelineParallelImpl(concurrencpp::executor_tag,
                                                          std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                                                          std::shared_ptr<Pipeline<ValueType>> pipeline) {
  co_await RunPipelineImpl(tpe, pipeline);
}

template<typename ValueType>
inline concurrencpp::result<void> RunPipelinesImpl(std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                                                   std::shared_ptr<std::vector<std::shared_ptr<Pipeline<ValueType>>>> pipelines) {
  throw_if_empty(tpe, TraceException::kResumeExecutorNull, source_loc::current());
  throw_if_empty(pipelines, "vector is null", source_loc::current());

  std::vector<concurrencpp::result<void>> tasks(pipelines->size());
  for (int index = 0; index < pipelines->size(); index++) {
    std::shared_ptr<Pipeline<ValueType>> pipeline = (*pipelines)[index];
    throw_if_empty(pipeline, TraceException::kPipelineNull, source_loc::current());

    tasks[index] = RunPipelineParallelImpl({}, tpe, pipeline);
  }

  co_await concurrencpp::when_all(tpe, tasks.begin(), tasks.end()).run();
  co_return;
}

template<typename ValueType>
inline void RunPipelines(std::shared_ptr<concurrencpp::thread_pool_executor> tpe,
                         std::shared_ptr<std::vector<std::shared_ptr<Pipeline<ValueType>>>> pipelines) {
  spdlog::info("start a pipeline");
  RunPipelinesImpl(tpe, pipelines).get();
  spdlog::info("finished a pipeline");
}

template<typename ValueType>
inline concurrencpp::result<void> run_pipeline_impl(
    std::shared_ptr<concurrencpp::executor> executor,
    pipeline<ValueType> &pipeline) {
  throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());

  spdlog::info("start a pipeline");

  const size_t amount_channels = pipeline.pipes_.size() + 1;
  std::vector<std::shared_ptr<CoroChannel<ValueType>>> channels{amount_channels};
  std::vector<concurrencpp::result<void>> tasks{amount_channels + 1};
  channels[0] = create_shared<CoroBoundedChannel<ValueType>>(TraceException::kChannelIsNull);
  throw_if_empty(pipeline.prod_, TraceException::kProducerIsNull, source_loc::current());
  tasks[0] = pipeline.prod_->produce(executor, channels[0]);

  for (size_t index = 0; index < pipeline.pipes_.size(); index++) {
    auto &pipe = pipeline.pipes_[index];
    throw_if_empty(pipe, TraceException::kPipeIsNull, source_loc::current());

    channels[index + 1] =
        create_shared<CoroBoundedChannel<ValueType>>(TraceException::kChannelIsNull);
    tasks[index + 1] =
        pipe->process(executor, channels[index], channels[index + 1]);
  }

  throw_if_empty(pipeline.cons_, TraceException::kConsumerIsNull, source_loc::current());
  tasks[amount_channels] =
      pipeline.cons_->consume(executor, channels[amount_channels - 1]);

  //co_await concurrencpp::when_all(executor, tasks.begin(), tasks.end()).run();
  for (size_t index = 0; index < amount_channels; index++) {
    co_await tasks[index];
    co_await channels[index]->CloseChannel(executor);
  }
  co_await tasks[amount_channels];

  spdlog::info("finished a pipeline");

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
  } catch (TraceException &exe) {
    std::cerr << exe.what() << '\n';
    executor->shutdown();
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
  } catch (TraceException &exe) {
    std::cerr << exe.what() << '\n';
    executor->shutdown();
    exit(EXIT_FAILURE);
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
  } catch (TraceException &exe) {
    std::cerr << exe.what() << '\n';
    executor->shutdown();
    exit(EXIT_FAILURE);
  }
}

template<typename ValueType>
inline void run_pipelines_parallel(
    std::shared_ptr<concurrencpp::executor> executor,
    std::vector<pipeline<ValueType>> &pipelines) {
  throw_if_empty(executor, TraceException::kResumeExecutorNull, source_loc::current());
  size_t amount_tasks = pipelines.size();
  //std::vector<concurrencpp::result<concurrencpp::result<void>>> pipelns{
  //    amount_tasks};
  std::vector<concurrencpp::result<void>> pipelns{amount_tasks};

  spdlog::info("start running pipelines in parallel");
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
    }
  } catch (TraceException &exe) {
    std::cerr << exe.what() << '\n';
    executor->shutdown();
    exit(EXIT_FAILURE);
  }

  spdlog::info("all pipelines finished");
}

inline void await_results(std::vector<concurrencpp::result<void>> &results) {
  for (auto &res : results) {
    res.get();
  }
}

#endif  // SIMBRICKS_TRACE_COROBELT_H_
