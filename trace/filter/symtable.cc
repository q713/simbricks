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

#include "trace/filter/symtable.h"

#include <functional>

#include "lib/utils/string_util.h"
#include "trace/reader/reader.h"

bool SymsFilter::parse_address(uint64_t &address) {
  line_reader_.trimL();
  if (!line_reader_.parse_uint_trim(16, address)) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not parse address out of line '%s'\n",
             identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
    return false;
  }
  return true;
}

bool SymsFilter::parse_name(std::string &name) {
  line_reader_.trimL();
  name = line_reader_.extract_and_substr_until(sim_string_utils::is_alnum_dot_bar);

  if (name.empty()) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not parse non empty name\n", identifier_.c_str());
#endif
    return false;
  }

  return true;
}

bool SymsFilter::add_to_sym_table(uint64_t address, const std::string &name) {
  auto in_set = symbol_filter_.find(name);
  if (!symbol_filter_.empty() && in_set == symbol_filter_.end()) {
#ifdef SYMS_DEBUG_
    DFLOGIN("%s: filter out symbol with name '%s'\n", identifier_.c_str(),
            name.c_str());
#endif
    return false;
  }

  auto pair = symbol_table_.insert(std::make_pair(address, name));
  if (!pair.second) {
#ifdef SYMS_DEBUG_
    DFLOGWARN("%s: could not insert new symbol table value at address '%u'\n",
              identifier_.c_str(), address);
#endif
    return false;
  }

  return true;
}

bool SymsFilter::filter(uint64_t address, std::string &sym_name_target) {
  auto symbol = symbol_table_.find(address);
  if (symbol != symbol_table_.end()) {
    sym_name_target = symbol->second;
    return true;
  }

  return false;
}

bool SymsSyms::skip_fags() {
  line_reader_.trimL();
  // flags are devided into 7 groups
  if (line_reader_.cur_length() < 8) {
#ifdef SYMS_DEBUG_
    DFLOGWARN(
        "%s: line has not more than 7 chars (flags), hence it is the wrong "
        "format",
        identifier_.c_str());
#endif
    return false;
  }
  line_reader_.move_forward(7);
  return true;
}

bool SymsSyms::skip_section() {
  line_reader_.trimL();
  line_reader_.trimTillWhitespace();
  return true;
}

bool SymsSyms::skip_alignment() {
  line_reader_.trimL();
  line_reader_.trimTillWhitespace();
  return true;
}

bool SymsSyms::load_file(const std::string &file_path) {
  if (!line_reader_.open_file(file_path)) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return false;
  }

  uint64_t address = 0;
  std::string name = "";
  while (line_reader_.next_line()) {
    line_reader_.trimL();

    // parse address
    if (!parse_address(address)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    // skip yet uninteresting values of ELF format
    if (!skip_fags() || !skip_section() || !skip_alignment()) {
#ifdef SYMS_DEBUG_
      DFLOGWARN(
          "%s: line '%s' seems to have wrong format regarding flags, section "
          "or alignment\n",
          identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    // parse name
    if (!parse_name(name)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse name from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (!add_to_sym_table(address, name)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not insert new val '[%u] = %s' into sym table\n",
                identifier_.c_str(), address, name.c_str());
#endif
    }
  }
  return true;
}

bool SSyms::load_file(const std::string &file_path) {
  if (!line_reader_.open_file(file_path)) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return false;
  }

  uint64_t address = 0;
  std::string label = "";
  while (line_reader_.next_line()) {
#ifdef SYMS_DEBUG_
    DFLOGIN("%s: found line: %s\n", identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
    line_reader_.trimL();

    // parse address
    if (!parse_address(address)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line '%s'\n",
                identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
      continue;
    }

    if (line_reader_.consume_and_trim_string(" <")) {
      if (parse_name(label)) {
        if (!line_reader_.consume_and_trim_char('>')) {
#ifdef SYMS_DEBUG_
          DFLOGERR(
              "%s: could not parse label from line '%s'\n",
              identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
          return false;
        }
      } else {
#ifdef SYMS_DEBUG_
        DFLOGERR("%s: could not parse label from line '%s'\n",
                 identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        return false;
      }
    } else {
      if (!line_reader_.consume_and_trim_char(':')) {
        label = "";
#ifdef SYMS_DEBUG_
        DFLOGWARN(
            "%s: could neiter parse label nor body addresses from line '%s'\n",
            identifier_.c_str(), line_reader_.get_raw_line().c_str());
#endif
        continue;
      }
    }

    if (!add_to_sym_table(address, label)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not insert new val '[%u] = %s' into sym table\n",
                identifier_.c_str(), address, label.c_str());
#endif
    }
  }
  return true;
}