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
#include "trace/events/eventStreamOperators.h"
#include "trace/events/events.h"
#include "trace/filter/symtable.h"
#include "trace/parser/parser.h"
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
  cxxopts::Options options("trace", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")(
      "linux-dump-server-client",
      "file path to a output file obtained by 'objdump --syms linux_image'",
      cxxopts::value<std::string>())(
      "gem5-log-server", "file path to a server log file written by gem5",
      cxxopts::value<std::string>())(
      "nicbm-log-server", "file path to a server log file written by the nicbm",
      cxxopts::value<std::string>())(
      "gem5-log-client", "file path to a client log file written by gem5",
      cxxopts::value<std::string>())(
      "nicbm-log-client", "file path to a client log file written by the nicbm",
      cxxopts::value<std::string>());

  cxxopts::ParseResult result;
  try {
    result = options.parse(argc, argv);

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (!result.count("linux-dump-server-client") ||
      !result.count("gem5-log-server") || !result.count("nicbm-log-server") ||
      !result.count("gem5-log-client") || !result.count("nicbm-log-client")) {
    std::cerr << "invalid arguments given" << std::endl
              << options.help() << std::endl;
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
  SymsSyms syms_filter{"SymbolTable-Client-Server", ssymsLr};
  if (!syms_filter.load_file(
          result["linux-dump-server-client"].as<std::string>())) {
    std::cerr << "could not initialize symbol table" << std::endl;
    exit(EXIT_FAILURE);
  }

  // component filter to only parse events written from certain components
  ComponentFilter compF("ComponentFilter-Client-Server");

  // gem5 log parser that generates events
  LineReader serverLr;
  Gem5Parser gem5ServerPar{"Gem5ServerParser",
                           result["gem5-log-server"].as<std::string>(),
                           syms_filter, compF, serverLr};
  LineReader clientLr;
  Gem5Parser gem5ClientPar{"Gem5ClientParser",
                           result["gem5-log-client"].as<std::string>(),
                           syms_filter, compF, clientLr};

  // nicbm log parser that generates events
  LineReader nicSerLr;
  NicBmParser nicSerPar{"NicbmServerParser",
                        result["nicbm-log-server"].as<std::string>(), nicSerLr};
  LineReader nicCliLr;
  NicBmParser nicCliPar{"NicbmClientParser",
                        result["nicbm-log-client"].as<std::string>(), nicCliLr};

  // printer to consume pipeline and to print events
  EventPrinter eventPrinter;

  // filter events out of stream
  EventTypeFilter eventFilter{
      {EventType::HostCall_t, EventType::HostMmioImRespPoW_t,
       EventType::HostMmioCR_t, EventType::HostMmioCW_t, EventType::HostMmioR_t,
       EventType::HostMmioW_t, EventType::NicMsix_t, EventType::NicDma_t,
       EventType::SetIX_t, EventType::NicDmaI_t, EventType::NicDmaEx_t,
       EventType::NicDmaEn_t, EventType::NicDmaCR_t, EventType::NicDmaCW_t,
       EventType::NicMmioR_t, EventType::NicMmioW_t, EventType::NicTx_t,
       EventType::NicRx_t}};
  EventTimestampFilter timestampFilter {
    EventTimestampFilter::EventTimeBoundary {
      EventTimestampFilter::EventTimeBoundary::MIN_LOWER_BOUND,
          EventTimestampFilter::EventTimeBoundary::MAX_UPPER_BOUND
    }
  };
  EventTypeStatistics statistics{{EventType::NicMmioR_t, EventType::NicMmioW_t,
                                  EventType::HostCall_t, EventType::NicDma_t,
                                  EventType::NicDmaEn_t, EventType::NicDmaCR_t,
                                  EventType::NicDmaCW_t, EventType::NicMmioR_t,
                                  EventType::NicMmioW_t, EventType::NicMsix_t}};

  // colelctor that merges event pipelines together in order of the given
  // comparator
  corobelt::Collector<std::shared_ptr<Event>, EventComperator> collector{
      {&nicSerPar, &nicCliPar, &gem5ServerPar, &gem5ClientPar}};

  corobelt::Pipeline<std::shared_ptr<Event>> pipeline{
      &collector, {&timestampFilter, &eventFilter, &statistics}};

  // an awaiter to wait for the termination of the parsing + printing pipeline
  // that means the awaiter is used to block till all events are processed
  corobelt::Awaiter<std::shared_ptr<Event>> awaiter(&pipeline, &eventPrinter);
  if (!awaiter.await_termination()) {
    std::cerr << "could not await termination of the pipeline" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::cout << statistics << std::endl;

  exit(EXIT_SUCCESS);
}