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

#include <iostream>

#include "util/cxxopts.hpp"
#include "env/traceEnvironment.h"
#include "reader/reader.h"
#include "concurrencpp/concurrencpp.h"

using ExhaustFuncT = std::function<concurrencpp::result<void>()>;
using ExecutorT = std::shared_ptr<concurrencpp::thread_pool_executor>;

template<size_t LineBufferSize>
requires SizeLagerZero<LineBufferSize>
ExhaustFuncT MakeExhaustTask(ReaderBuffer<LineBufferSize> &reader_buffer,
                             const std::string &file_path,
                             bool is_named_pipe) {
  ExhaustFuncT task = [
      &buffer = reader_buffer,
      &path = file_path,
      is_pipe = is_named_pipe
  ]() -> concurrencpp::result<void> {
    buffer.OpenFile(path, is_pipe);
    std::pair<bool, LineHandler *> handler;
    for (handler = buffer.NextHandler();
         handler.second and handler.second != nullptr;
         handler = buffer.NextHandler());
    co_return;
  };
  return task;
}

int main(int argc, char *argv[]) {

  cxxopts::Options options("exhauster", "Tool to Exhaust Log-File or Named-Pipe");
  options.add_options()("h,help", "Print usage")
      ("gem5-log-server", "file path to a server log file written by gem5", cxxopts::value<std::string>())
      ("nicbm-log-server", "file path to a server log file written by the nicbm", cxxopts::value<std::string>())
      ("gem5-log-client", "file path to a client log file written by gem5", cxxopts::value<std::string>())
      ("nicbm-log-client", "file path to a client log file written by the nicbm", cxxopts::value<std::string>())
      ("ns3-log", "file path to a log file written by ns3", cxxopts::value<std::string>());

  cxxopts::ParseResult result;
  try {
    result = options.parse(argc, argv);

  } catch (cxxopts::exceptions::exception &e) {
    std::cerr << "Could not parse cli options: " << e.what() << '\n';
    exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << '\n';
    exit(EXIT_SUCCESS);
  }

  if (!result.count("gem5-log-server") ||
      !result.count("nicbm-log-server") ||
      !result.count("gem5-log-client") ||
      !result.count("nicbm-log-client") ||
      !result.count("ns3-log")) {
    std::cerr << "invalid arguments given" << '\n'
              << options.help() << '\n';
    exit(EXIT_FAILURE);
  }

  constexpr size_t kLineBufferSize = 1;
  const concurrencpp::runtime runtime;
  const ExecutorT executor = runtime.thread_pool_executor();
  std::array<concurrencpp::result<concurrencpp::result<void>>, 5> tasks;

  std::cout << "start running exhaustion" << '\n';

  // SERVER HOST PIPELINE
  ReaderBuffer<kLineBufferSize> gem5_ser_buf_pro{"Gem5ServerEventProvider", true};
  auto gem5_ser_task = MakeExhaustTask<kLineBufferSize>(gem5_ser_buf_pro,
                                                        result["gem5-log-server"].as<std::string>(),
                                                        true);
  tasks[0] = executor->submit(gem5_ser_task);

  // CLIENT HOST PIPELINE
  ReaderBuffer<kLineBufferSize> gem5_client_buf_pro{"Gem5ClientEventProvider", true};
  auto gem5_client_task = MakeExhaustTask<kLineBufferSize>(gem5_client_buf_pro,
                                                           result["gem5-log-client"].as<std::string>(),
                                                           true);
  tasks[1] = executor->submit(gem5_client_task);

  // SERVER NIC PIPELINE
  ReaderBuffer<kLineBufferSize> nicbm_ser_buf_pro{"NicbmServerEventProvider", true};
  auto nicbm_ser_task = MakeExhaustTask<kLineBufferSize>(nicbm_ser_buf_pro,
                                                         result["nicbm-log-server"].as<std::string>(),
                                                         true);
  tasks[2] = executor->submit(nicbm_ser_task);

  // CLIENT NIC PIPELINE
  ReaderBuffer<kLineBufferSize> nicbm_client_buf_pro{"NicbmClientEventProvider", true};
  auto nicbm_client_task = MakeExhaustTask<kLineBufferSize>(nicbm_client_buf_pro,
                                                            result["nicbm-log-client"].as<std::string>(),
                                                            true);
  tasks[3] = executor->submit(nicbm_client_task);

  // NS3 PIPELINE
  ReaderBuffer<kLineBufferSize> ns3_buf_pro{"Ns3EventProvider", true};
  auto ns3_task = MakeExhaustTask<kLineBufferSize>(ns3_buf_pro,
                                                        result["ns3-log"].as<std::string>(),
                                                        true);
  tasks[4] = executor->submit(ns3_task);

  for (auto &task : tasks) {
    task.get().get();
  }

  std::cout << "finished exhaustion" << '\n';

  exit(EXIT_SUCCESS);
}

