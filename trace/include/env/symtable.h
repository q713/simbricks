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

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <map>
#include <utility>

#include "util/log.h"
#include "util/utils.h"
#include "reader/reader.h"
#include "reader/cReader.h"
#include "env/stringInternalizer.h"

enum FilterType { kSyms, kS, kElf };

inline FilterType FilterTypeFromString(const std::string &type_str) {
  static const std::unordered_map<std::string, FilterType> kLookup{
      {"kSyms", kSyms},
      {"kS", kS},
      {"kElf", kElf}
  };
  auto iter = kLookup.find(type_str);
  if (iter != kLookup.end()) {
    return iter->second;
  }
  std::cerr << "could not resolve filter type string '" << type_str << "'. Fallback to kSyms" << '\n';
  return FilterType::kSyms;
}

class SymsFilter {
 public:
  using SymTableT = std::map<uint64_t, const std::string *>;
  //using SymTableT = std::unordered_map<uint64_t, const std::string *>;

 protected:
  uint64_t id_;
  const std::string component_;
  ReaderBuffer<MultiplePagesBytes(4)> reader_buffer_;
  //LineReader line_reader_;
  std::set<std::string> symbol_filter_;
  //SymTableT symbol_table_;
  SymTableT symbol_table_;
  StringInternalizer &i_;

  explicit SymsFilter(uint64_t id, const std::string component,
                      std::set<std::string> symbol_filter,
                      StringInternalizer &i)
      : id_(id),
        component_(component),
        //reader_buffer_("symtable", true),
        reader_buffer_("symtable"),
        symbol_filter_(std::move(symbol_filter)),
        i_(i) {};

  bool ParseAddress(LineHandler &line_handler, uint64_t &address);

  bool ParseName(LineHandler &line_handler, std::string &name);

  bool AddToSymTable(uint64_t address, const std::string &name,
                     uint64_t address_offset);

  bool SkipSymsFags(LineHandler &line_handler);

  bool SkipSymsSection(LineHandler &line_handler);

  bool SkipSymsAlignment(LineHandler &line_handler);

  /*
   * This class is used to parse a symbol table given in ELF format.
   * This could look as follows:
   *  address:  flags:  section: alignment: name:
   *  00000000  l    d  .bss     00000000   .bss
   * Such a symbol table is for example generated by using 'objdump --syms
   * linux_image'. It is later used to translate gem5 output into a human
   * readble form.
   */
  bool LoadSyms(const std::string &file_path, uint64_t address_offset);

  /*
   * This class is used to parse a symbol table given in the following format:
   * ffffffff812c56ea <tty_set_termios>:
   * ffffffff812c56ea:       41 55                   push   %r13
   * ffffffff812c56ec:       41 54                   push   %r12
   * Such a symbol table is for example generated by using 'objdump -S
   * linux_image'. It is later used to translate gem5 output into a human
   * readble form.
   */
  bool LoadS(const std::string &file_path, uint64_t address_offset);

  bool LoadElf(const std::string &file_path, uint64_t address_offset);

#if 0
  bool ParseAddress(uint64_t &address);

  bool ParseName(std::string &name);

  bool AddToSymTable(uint64_t address, const std::string &name,
                     uint64_t address_offset);

  bool SkipSymsFags();

  bool SkipSymsSection();

  bool SkipSymsAlignment();

  /*
   * This class is used to parse a symbol table given in ELF format.
   * This could look as follows:
   *  address:  flags:  section: alignment: name:
   *  00000000  l    d  .bss     00000000   .bss
   * Such a symbol table is for example generated by using 'objdump --syms
   * linux_image'. It is later used to translate gem5 output into a human
   * readble form.
   */
  bool LoadSyms(const std::string &file_path, uint64_t address_offset);

  /*
   * This class is used to parse a symbol table given in the following format:
   * ffffffff812c56ea <tty_set_termios>:
   * ffffffff812c56ea:       41 55                   push   %r13
   * ffffffff812c56ec:       41 54                   push   %r12
   * Such a symbol table is for example generated by using 'objdump -S
   * linux_image'. It is later used to translate gem5 output into a human
   * readble form.
   */
  bool LoadS(const std::string &file_path, uint64_t address_offset);

  bool LoadElf(const std::string &file_path, uint64_t address_offset);
#endif
 public:
  inline uint64_t &GetIdent() {
    return id_;
  }

  inline const std::string &GetComponent() {
    return component_;
  }

  SymTableT &GetSymTable() {
    return symbol_table_;
  }

  /*
   * Filter function for later usage in parser.
   * Get in address and receive label for address.
   */
  const std::string *Filter(uint64_t address);

  const std::string *FilterNearestAddressUpper(uint64_t address);

  const std::string *FilterNearestAddressLower(uint64_t address);

  static std::shared_ptr<SymsFilter> Create(uint64_t id,
                                            const std::string component,
                                            const std::string &file_path,
                                            uint64_t address_offset,
                                            FilterType type,
                                            StringInternalizer &i) {
    return SymsFilter::Create(id, component, file_path,
                              address_offset, type, {}, i);
  }

  static std::shared_ptr<SymsFilter> Create(uint64_t id,
                                            const std::string component,
                                            const std::string &file_path,
                                            uint64_t address_offset,
                                            FilterType type,
                                            std::set<std::string> symbol_filter,
                                            StringInternalizer &i);

  friend std::ostream &operator<<(std::ostream &os, SymsFilter &filter) {
    os << std::endl << std::endl;
    os << "Symbol Table Filter:";
    os << std::endl << std::endl;
    auto &table = filter.GetSymTable();
    os << "There were " << table.size() << " many entries found";
    os << std::endl << std::endl;
    for (auto &entry : table) {
      os << "[" << std::hex << entry.first << "] = ";
      os << (entry.second ? *(entry.second) : "null");
      os << std::endl;
    }
    os << std::endl;
    return os;
  }
};

#endif  // SIMBRICKS_TRACE_SYMS_H_