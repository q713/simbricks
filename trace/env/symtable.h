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

#ifndef SIMBRICKS_TRACE_SYMS_H_
#define SIMBRICKS_TRACE_SYMS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "lib/utils/log.h"
#include "trace/env/stringInternalizer.h"
#include "trace/reader/reader.h"

enum FilterType { Syms, S, Elf };

class SymsFilter {
 protected:
  const std::string identifier_;
  LineReader line_reader_;
  std::set<std::string> symbol_filter_;
  std::map<uint64_t, const std::string *> symbol_table_;
  string_internalizer &i_;

  explicit SymsFilter(const std::string identifier,
                      std::set<std::string> symbol_filter,
                      string_internalizer &i)
      : identifier_(std::move(identifier)),
        line_reader_(true),
        symbol_filter_(std::move(symbol_filter)),
        i_(i){};

  bool parse_address(uint64_t &address);

  bool parse_name(std::string &name);

  bool add_to_sym_table(uint64_t address, const std::string &name,
                        uint64_t address_offset);

  bool skip_syms_fags();

  bool skip_syms_section();

  bool skip_syms_alignment();

  /*
   * This class is used to parse a symbol table given in ELF format.
   * This could look as follows:
   *  address:  flags:  section: alignment: name:
   *  00000000  l    d  .bss     00000000   .bss
   * Such a symbol table is for example generated by using 'objdump --syms
   * linux_image'. It is later used to translate gem5 output into a human
   * readble form.
   */
  bool load_syms(const std::string &file_path, uint64_t address_offset);

  /*
   * This class is used to parse a symbol table given in the following format:
   * ffffffff812c56ea <tty_set_termios>:
   * ffffffff812c56ea:       41 55                   push   %r13
   * ffffffff812c56ec:       41 54                   push   %r12
   * Such a symbol table is for example generated by using 'objdump -S
   * linux_image'. It is later used to translate gem5 output into a human
   * readble form.
   */
  bool load_s(const std::string &file_path, uint64_t address_offset);

  bool load_elf(const std::string &file_path, uint64_t address_offset);

 public:
  inline const std::string *get_ident() {
    return &identifier_;
  }

  inline const std::map<uint64_t, const std::string *> &get_sym_table() {
    return symbol_table_;
  }

  /*
   * Filter function for later usage in parser.
   * Get in address and receive label for address.
   */
  const std::string *filter(uint64_t address);

  static std::shared_ptr<SymsFilter> create(const std::string identifier,
                                            const std::string &file_path,
                                            uint64_t address_offset,
                                            FilterType type,
                                            string_internalizer &i) {
    return SymsFilter::create(std::move(identifier), file_path, address_offset,
                              type, {}, i);
  }

  static std::shared_ptr<SymsFilter> create(const std::string identifier,
                                            const std::string &file_path,
                                            uint64_t address_offset,
                                            FilterType type,
                                            std::set<std::string> symbol_filter,
                                            string_internalizer &i);

  friend std::ostream &operator<<(std::ostream &os, SymsFilter &filter) {
    os << std::endl << std::endl;
    os << "Symbol Table Filter:";
    os << std::endl << std::endl;
    auto &table = filter.get_sym_table();
    os << "There were " << table.size() << " many entries found";
    os << std::endl << std::endl;
    for (auto &entry : table) {
      os << "[" << std::hex << entry.first << "] = " << entry.second
         << std::endl;
    }
    os << std::endl;
    return os;
  }
};

#endif  // SIMBRICKS_TRACE_SYMS_H_