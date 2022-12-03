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

symtable::addressopt_t symtable::SymsFilter::parse_address(std::string &line) {
  sim_string_utils::trimL(line);
  address_t address;
  if (!sim_string_utils::parse_uint_trim(line, 16, &address)) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not parse address out of line '%s'\n",
             identifier_.c_str(), line.c_str());
#endif
    return std::nullopt;
  }
  return address;
}

symtable::nameopt_t symtable::SymsFilter::parse_name(std::string &line) {
  static std::function<bool(unsigned char)> is_part_name = [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '.';
  };

  sim_string_utils::trimL(line);
  std::string name =
      sim_string_utils::extract_and_substr_until(line, is_part_name);

  if (name.empty()) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not parse non empty name\n", identifier_.c_str());
#endif
    return std::nullopt;
  }

  return name;
}

bool symtable::SymsFilter::add_to_sym_table(symtable::address_t address,
                                            const name_t &name) {
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

symtable::filter_ret_t symtable::SymsFilter::filter(uint64_t address) {
  auto symbol = symbol_table_.find(address);
  if (symbol != symbol_table_.end()) {
    return symbol->second;
  }

  return std::nullopt;
}

bool symtable::SymsSyms::skip_fags(std::string &line) {
  sim_string_utils::trimL(line);
  // flags are devided into 7 groups
  if (line.length() < 8) {
#ifdef SYMS_DEBUG_
    DFLOGWARN(
        "%s: line has not more than 7 chars (flags), hence it is the wrong "
        "format",
        identifier_.c_str());
#endif
    return false;
  }
  line = line.substr(7);
  return true;
}

bool symtable::SymsSyms::skip_section(std::string &line) {
  sim_string_utils::trimL(line);
  sim_string_utils::trimTillWhitespace(line);
  return true;
}

bool symtable::SymsSyms::skip_alignment(std::string &line) {
  sim_string_utils::trimL(line);
  sim_string_utils::trimTillWhitespace(line);
  return true;
}

bool symtable::SymsSyms::load_file(const std::string &file_path) {
  auto reader_opt = reader::LineReader::create(log_file_path);
  if (!reader_opt.has_value()) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not create reader\n", identifier_.c_str());
#endif
    return false;
  }
  auto reader = std::move(reader_opt.value());

  for (std::string line; reader.get_next_line(line, true);) {
    sim_string_utils::trim(line);

    // parse address
    symtable::addressopt_t address_opt = parse_address(line);
    if (!address_opt.has_value()) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line with number: %d\n",
                identifier_.c_str(), reader.ln());
#endif
      continue;
    }
    address_t address = address_opt.value();

    // skip yet uninteresting values of ELF format
    if (!skip_fags(line) || !skip_section(line) || !skip_alignment(line)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN(
          "%s: line '%s' seems to have wrong format regarding flags, section "
          "or alignment\n",
          identifier_.c_str(), line.c_str());
#endif
      continue;
    }

    // parse name
    symtable::nameopt_t name_opt = parse_name(line);
    if (!name_opt.has_value()) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse name from line with number: %d\n",
                identifier_.c_str(), reader.ln());
#endif
      continue;
    }
    symtable::name_t name = name_opt.value();

    if (!add_to_sym_table(address, name)) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not insert new val '[%u] = %s' into sym table\n",
                identifier_.c_str(), address, name.c_str());
#endif
    }
  }
  return true;
}

bool symtable::SSyms::load_file(const std::string &file_path) {
  reader::LineReader reader(file_path);

  if (!reader.is_valid()) {
#ifdef SYMS_DEBUG_
    DFLOGERR("%s: could not open file with path '%s'\n", identifier_.c_str(),
             file_path.c_str());
#endif
    return false;
  }

  std::string label = "";
  for (std::string line; reader.get_next_line(line, true);) {
#ifdef SYMS_DEBUG_
    DFLOGIN("%s: found line: %s\n", identifier_.c_str(), line.c_str());
#endif
    sim_string_utils::trim(line);

    // parse address
    symtable::addressopt_t address_opt = parse_address(line);
    if (!address_opt.has_value()) {
#ifdef SYMS_DEBUG_
      DFLOGWARN("%s: could not parse address from line with number: %d\n",
                identifier_.c_str(), reader.ln());
#endif
      continue;
    }
    symtable::address_t address = address_opt.value();

    if (sim_string_utils::consume_and_trim_string(line, " <")) {
      symtable::nameopt_t label_opt = parse_name(line);
      if (label_opt.has_value()) {
        if (!sim_string_utils::consume_and_trim_char(line, '>')) {
#ifdef SYMS_DEBUG_
          DFLOGERR(
              "%s: could not parse label from line %d, unexpected format\n",
              identifier_.c_str(), reader.ln());
#endif
          return false;
        }
        label = label_opt.value();
      } else {
#ifdef SYMS_DEBUG_
        DFLOGERR("%s: could not parse label from line %d\n",
                 identifier_.c_str(), reader.ln());
#endif
        return false;
      }
    } else {
      if (!sim_string_utils::consume_and_trim_char(line, ':')) {
        label = "";
#ifdef SYMS_DEBUG_
        DFLOGWARN(
            "%s: could neiter parse label nor body addresses from line %d\n",
            identifier_.c_str(), reader.ln());
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