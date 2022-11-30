#include <algorithm>
#include <climits>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>

#include "lib/utils/cxxopts.hpp"
#include "lib/utils/log.h"
#include "lib/utils/string_util.h"
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
      return EXIT_SUCCESS;
    }

    if (!result.count("linux-dump")) {
      DLOGERR("could not parse option 'linux-dump'\n");
      return EXIT_FAILURE;
    }

  } catch (cxxopts::exceptions::exception &e) {
    DFLOGERR("Could not parse cli options: %s\n", e.what());
    return EXIT_FAILURE;
  }

  SymsSyms syms_filter = SymsSyms{};
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

  if (!syms_filter.load_file(linux_dump)) {
    DFLOGERR("could not load file with path '%s'\n", linux_dump);
    return EXIT_FAILURE;
  }

  for (auto it = syms_filter.symbol_table_.begin(); it != syms_filter.symbol_table_.end(); it++) {
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
  return 0;
}