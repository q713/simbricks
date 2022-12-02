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

#pragma once

#define SYMS_DEBUG_ 1

#include <map>
#include <optional>
#include <set>
#include <string>

#include "lib/utils/log.h"

namespace symtable {

using filter_ret_t = std::optional<std::string>;
using address_t = uint64_t;
using addressopt_t = std::optional<address_t>;
using name_t = std::string;
using nameopt_t = std::optional<name_t>;
using symtable_t = std::map<address_t, name_t>;

class SymsFilter {
 protected:
  const std::string identifier_;
  std::set<name_t> symbol_filter_;
  symtable_t symbol_table_;

  addressopt_t parse_address(std::string &line);

  nameopt_t parse_name(std::string &line);

  bool add_to_sym_table(address_t address, const name_t &name);

 public:
  explicit SymsFilter(const std::string identifier) : identifier_(identifier){};

  /*
   * Builder function to add symbols to the symbol filter.
   * After parsing the symbol table, only those symbols are included
   * in the resulting symbol table that is used to translate addresses
   * into symbols. That way this translation also acts as a filter.
   */
  inline SymsFilter &operator()(const name_t &symbol) {
    auto res = symbol_filter_.insert(symbol);
    if (!res.second) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could no insert '%s' into symbol map\n",
                identifier_.c_str(), symbol.c_str());
#endif
    }
    return *this;
  }

  inline const symtable_t &get_sym_table() {
    return symbol_table_;
  }

  /*
   * Filter function for later usage in parser.
   * Get in address and receive label for address.
   */
  filter_ret_t filter(uint64_t address);

  virtual bool load_file(const std::string &file_path) = 0;
};

/*
 * This class is used to parse a symbol table given in ELF format.
 * This could look as follows:
 *  address:  flags:  section: alignment: name:
 *  00000000  l    d  .bss     00000000   .bss
 * Such a symbol table is for example generated by using 'objdump --syms
 * linux_image'. It is later used to translate gem5 output into a human readble
 * form.
 */
class SymsSyms : public SymsFilter {
 protected:
  bool skip_fags(std::string &line);

  bool skip_section(std::string &line);

  bool skip_alignment(std::string &line);

 public:
  explicit SymsSyms(const std::string identifier) : SymsFilter(identifier) {
  }

  bool load_file(const std::string &file_path) override;
};

/*
 * This class is used to parse a symbol table given in the following format:
 * ffffffff812c56ea <tty_set_termios>:
 * ffffffff812c56ea:       41 55                   push   %r13
 * ffffffff812c56ec:       41 54                   push   %r12
 * Such a symbol table is for example generated by using 'objdump -S
 * linux_image'. It is later used to translate gem5 output into a human readble
 * form.
 */
class SSyms : public SymsFilter {
 public:
  explicit SSyms(const std::string identifier) : SymsFilter(identifier) {
  }

  bool load_file(const std::string &file_path) override;
};

}  // namespace symtable

#endif  // SIMBRICKS_TRACE_SYMS_H_