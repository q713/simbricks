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

#include "reader/reader.h"

bool LineReader::OpenFile(const std::string &file_path) {
  close_input();
  input_stream_.open(file_path, std::ios_base::in | std::ios_base::binary);
  if (!input_stream_.is_open()) {
    return false;
  }

  cur_reading_pos_ = 0;
  line_number_ = 0;

  return is_valid();
}

bool LineReader::NextLine() {
  bool found = false;
  while (!found) {
    if (input_stream_.eof() || !is_valid()) {
      return false;
    }

    std::getline(input_stream_, cur_line_);
    if (input_stream_.fail()) {
      return false;
    }

    ++line_number_;
  
    if (skip_empty_lines_ and cur_line_.empty()) {
      continue;
    } else {
      found = true;
    }
  }
  cur_reading_pos_ = 0;
  return true;
}

bool LineReader::MoveForward(size_t steps) {
  if (is_empty() || cur_length() < steps)
    return false;

  cur_reading_pos_ += steps;
  return true;
}

void LineReader::TrimL() {
  if (is_empty())
    return;

  auto till = std::find_if_not(cur_line_.begin() + cur_reading_pos_,
                               cur_line_.end(), sim_string_utils::is_space);
  if (till != cur_line_.end()) {
    cur_reading_pos_ = std::distance(cur_line_.begin(), till);
  }
}

void LineReader::TrimTillWhitespace() {
  if (is_empty())
    return;

  auto till = std::find_if(cur_line_.begin() + cur_reading_pos_,
                           cur_line_.end(), sim_string_utils::is_space);
  if (till != cur_line_.end())
    cur_reading_pos_ = std::distance(cur_line_.begin(), till);
}

std::string LineReader::ExtractAndSubstrUntil(
    std::function<bool(unsigned char)> &predicate) {
  std::stringstream extract_builder;
  while (!is_empty()) {
    unsigned char letter = cur_line_[cur_reading_pos_];
    if (!predicate(letter)) {
      break;
    }
    extract_builder << letter;
    ++cur_reading_pos_;
  }
  return extract_builder.str();
}

 bool LineReader::SkipTill(std::function<bool(unsigned char)> &predicate) {
  if (is_empty()) {
    return false;
  }

  auto begin = cur_line_.begin() + cur_reading_pos_;
  auto end = std::find_if(begin, cur_line_.end(), predicate);

  if (end != cur_line_.end()) {
    auto distance = std::distance(begin, end);
    cur_reading_pos_ += distance;
    return true;
  }

  return false;
 }

bool LineReader::TrimTillConsume(const std::string &tc, bool strict) {
  if (is_empty() || cur_length() < tc.length()) {
    return false;
  }

  auto sub_start = cur_line_.begin() + cur_reading_pos_;
  auto start = std::search(sub_start, cur_line_.end(), tc.begin(), tc.end());
  if (start == cur_line_.end() || (strict && start != sub_start)) {
    return false;
  }

  cur_reading_pos_ = std::distance(cur_line_.begin(), start + tc.length());
  return true;
}

bool LineReader::ConsumeAndTrimChar(const char to_consume) {
  if (is_empty()) {
    return false;
  }
  unsigned char letter = cur_line_[cur_reading_pos_];
  if (letter != to_consume)
    return false;

  ++cur_reading_pos_;
  return true;
}

bool LineReader::ParseUintTrim(int base, uint64_t &target) {
  if (is_empty() or (base != 10 and base != 16)) {
    return false;
  }
  auto pred =
      base == 10 ? sim_string_utils::is_num : sim_string_utils::is_alnum;

  const size_t old_reading_pos = cur_reading_pos_;
  const std::string num_string = ExtractAndSubstrUntil(pred);
  if (num_string.empty()) {
    cur_reading_pos_ = old_reading_pos;
    return false;
  }

  char *end;
  const uint64_t num = std::strtoul(num_string.c_str(), &end, base);
  if (num == ULONG_MAX) {
    cur_reading_pos_ = old_reading_pos;
    return false;
  }

  target = num;
  return true;
}

bool LineReader::ParseInt(int &target) {
  if (is_empty()) {
    return false;
  }

  const size_t old_reading_pos = cur_reading_pos_;
  const std::string num_string = ExtractAndSubstrUntil(sim_string_utils::is_num);
  if (num_string.empty()) {
    cur_reading_pos_ = old_reading_pos;
    return false;
  }

  target = std::stoi(num_string);
  return true;
}
