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

int main(int argc, char *argv[]) {
  std::string linux_dump;

  cxxopts::Options options("Tracing", "Log File Analysis/Tracing Tool");
  options.add_options()("h,help", "Print usage")(
      "linux-dump",
      "file path to a output file obtained by 'objdump --syms linux_image'",
      cxxopts::value<std::string>(linux_dump));

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

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    exit(EXIT_FAILURE);
  }

  /*
  symtable::SymsSyms syms_filter{"SymbolTableFilter"};
  syms_filter("__tpstrtab_vector_free_moved")
    ("__tpstrtab_vector_setup")
    ("__tpstrtab_vector_teardown")
    ("__tpstrtab_vector_deactivate")
    ("__tpstrtab_vector_activate")
    ("__tpstrtab_vector_alloc_managed")
    ("__tpstrtab_vector_alloc")
    ("__tpstrtab_vector_reserve")
    ("__tpstrtab_vector_reserve_managed")
    ("__tpstrtab_vector_clear")
    ("__tpstrtab_vector_update")
    ("__tpstrtab_vector_config")
    ("__tpstrtab_thermal_apic_exit")
    ("__tpstrtab_thermal_apic_entry");
    */

   symtable::SSyms syms_filter{"SymbolTableFilter"};
   syms_filter("_stext")
    ("secondary_startup_64")
    ("perf_trace_initcall_level")
    ("perf_trace_initcall_start")
    ("perf_trace_initcall_finish")
  ;

  if (!syms_filter.load_file(linux_dump)) {
    DFLOGERR("could not load file with path '%s'\n", linux_dump);
    exit(EXIT_FAILURE);
  }

  for (auto it = syms_filter.get_sym_table().begin(); it != syms_filter.get_sym_table().end(); it++) {
    std::cout << "[" << it->first << "]" << " = " << it->second << std::endl;
  }

  // TODO:
  //1) check for parsing 'objdump -S vmlinux'
  //2) which gem5 flags -> before witing parser --> use Exec without automatic translation + Syscall
  //3) gem5 parser
  //4) nicbm parser
  //5) merge events by timestamp
  //6) how should events look like?
  //7)add identifiers to know sources
  //8) try trace?  

  exit(EXIT_SUCCESS);
}