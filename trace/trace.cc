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

#include "lib/utils/cxxopts.hpp"
#include "lib/utils/log.h"
#include "trace/corobelt/belt.h"
#include "trace/filter/symtable.h"
#include "trace/events/events.h"
#include "trace/reader/reader.h"

class IntProd : public corobelt::Producer<int> {
  int id_;

 public:
  IntProd(int id) : corobelt::Producer<int>(), id_(id) {
  }

  void produce(corobelt::coro_push_t<int> &sink) override {
    for (int e = 0; e < 10; e++) {
      sink(id_);
    }
    return;
  }
};

class IntAdder : public corobelt::Pipe<int> {
 public:
  IntAdder() : corobelt::Pipe<int>() {
  }

  void process(corobelt::coro_push_t<int> &sink,
               corobelt::coro_pull_t<int> &source) override {
    for (int event : source) {
      event += 10;
      sink(event);
    }
    return;
  }
};

class IntPrinter : public corobelt::Consumer<int> {
 public:
  explicit IntPrinter() : corobelt::Consumer<int>() {
  }

  void consume(corobelt::coro_pull_t<int> &source) override {
    std::cout << "start to sinkhole events" << std::endl;
    for (int e : source) {
      std::cout << "consumed " << e << std::endl;
    }
    return;
  }
};

int main(int argc, char *argv[]) {
  
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

    // if (!result.count("linux-dump")) {
    //   DLOGERR("could not parse option 'linux-dump'\n");
    //   exit(EXIT_FAILURE);
    // }

    // if (!result.count("gem5-log")) {
    //   DLOGERR("could not parse option 'gem-5-log'\n");
    //   exit(EXIT_FAILURE);
    // }

    if (!result.count("nicbm-log")) {
      DLOGERR("could not parse option 'nicbm-log'\n");
      exit(EXIT_FAILURE);
    }

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  // SSyms syms_filter("SymbolTableFilter", ssymsLr);
  // syms_filter("entry_SYSCALL_64")
  //   ("__do_sys_gettimeofday")
  //   ("__sys_sendto")
  //   ("i40e_lan_xmit_frame")
  //   ("syscall_return_via_sysret")
  //   ("__sys_recvfrom")
  //   ("deactivate_task")
  //   ("interrupt_entry")
  //   ("i40e_msix_clean_rings")
  //   ("napi_schedule_prep")
  //   ("__do_softirq")
  //   ("trace_napi_poll")
  //   ("net_rx_action")
  //   ("i40e_napi_poll")
  //   ("activate_task")
  //   ("copyout");

  // symbol filter to translate hex address to function-name/label
  LineReader ssymsLr;
  SymsSyms syms_filter{"SymbolTableFilter", ssymsLr};

  // component filter to only parse events written from certain components
  ComponentFilter compF("Component Filter");
  
  // log parser that generates events
  LineReader gem5Lr;
  Gem5Parser gem5Par{"Gem5Parser", gem5_log, syms_filter, compF, gem5Lr};
  
  // printer to consume pipeline and to print events
  EventPrinter eventPrinter;

  // an awaiter to wait for the termination of the parsing + printing pipeline (a.k.a. to block)
  corobelt::Awaiter<std::shared_ptr<Event>> awaiter(&gem5Par, &eventPrinter);
  if (!awaiter.await_termination()) {
    std::cout << "could not await termination of the pipeline" << std::endl;
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}