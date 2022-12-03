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
#include "trace/filter/symtable.h"
#include "trace/parser/parser.h"

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

    if (!result.count("linux-dump")) {
      DLOGERR("could not parse option 'linux-dump'\n");
      exit(EXIT_FAILURE);
    }

    if (!result.count("gem5-log")) {
      DLOGERR("could not parse option 'gem-5-log'\n");
      exit(EXIT_FAILURE);
    }

    if (!result.count("nicbm-log")) {
      DLOGERR("could not parse option 'nicbm-log'\n");
      exit(EXIT_FAILURE);
    }

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  // symtable::SymsSyms syms_filter{"SymbolTableFilter"};
  // syms_filter("entry_SYSCALL_64")("__do_sys_gettimeofday")("__sys_sendto")(
  //     "i40e_lan_xmit_frame")("syscall_return_via_sysret")("__sys_recvfrom")(
  //     "deactivate_task")("interrupt_entry")("i40e_msix_clean_rings")(
  //     "napi_schedule_prep")("__do_softirq")("trace_napi_poll")("net_rx_action")(
  //     "i40e_napi_poll")("activate_task")("copyout");

  //symtable::SSyms syms_filter{"SymbolTableFilter"};
  //syms_filter("entry_SYSCALL_64")("__do_sys_gettimeofday")("__sys_sendto")(
  //    "i40e_lan_xmit_frame")("syscall_return_via_sysret")("__sys_recvfrom")(
  //    "deactivate_task")("interrupt_entry")("i40e_msix_clean_rings")(
  //    "napi_schedule_prep")("__do_softirq")("trace_napi_poll")("net_rx_action")(
  //    "i40e_napi_poll")("activate_task")("copyout");

  //if (!syms_filter.load_file(linux_dump)) {
  //  DFLOGERR("could not load file with path '%s'\n", linux_dump);
  //  exit(EXIT_FAILURE);
  //}

  //logparser::Gem5Parser gem5Par("Gem5Parser", syms_filter);
  //if (!gem5Par.parse(gem5_log)) {
  //  DFLOGERR("could not parse gem5 log file with path '%s'\n",
  //           linux_dump.c_str());
  //  exit(EXIT_FAILURE);
  //}

  logparser::NicBmParser nicBmPar("NicBmParser");
  if (!nicBmPar.parse(nicbm_log)) {
    DFLOGERR("could not parse nicbm log file with path '%s'\n", nicbm_log.c_str());
    exit(EXIT_FAILURE);
  }

  // TODO:
  // 1) check for parsing 'objdump -S vmlinux'
  // 2) which gem5 flags -> before witing parser --> use Exec without automatic
  // translation + Syscall 
  // 3) gem5 parser 
  // 4) nicbm parser
  // 5) handle symbricks events in all parsers 
  // 6) merge events by timestamp 
  // 7) how should events look like? 
  // 8) add identifiers to know sources
  // 9) try trace?
  // 10) default events for not parsed lines / unexpected lines?

  exit(EXIT_SUCCESS);
}