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

#include <filesystem>

bool LineReader::OpenFile(const std::string &file_path) {
  CloseInput();
  std::cout << "file_path: " << file_path << std::endl;
  if (!std::filesystem::exists(file_path)) {
    return false;
  }

  //input_stream_ = std::ifstream(file_path);
  input_stream_.open(file_path, std::ios_base::in | std::ios_base::binary);
  if (!input_stream_.is_open()) {
    return false;
  }

  cur_reading_pos_ = 0;
  line_number_ = 0;

  return IsValid();
}

bool LineReader::NextLine() {
  bool found = false;
  while (!found) {
    if (input_stream_.eof() || !IsValid()) {
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
  cur_line_it_ = cur_line_.begin();
  cur_line_end_it_ = cur_line_.end();
  return true;
}

bool LineReader::MoveForward(size_t steps) {
  if (IsEmpty() || CurLength() < steps)
    return false;

  cur_reading_pos_ += steps;
  return true;
}

void LineReader::TrimL() {
  if (IsEmpty())
    return;

  auto till = std::find_if_not(cur_line_it_ + cur_reading_pos_,
                               cur_line_end_it_, sim_string_utils::is_space);
  if (till != cur_line_end_it_) {
    cur_reading_pos_ = std::distance(cur_line_it_, till);
  }
}

void LineReader::TrimTillWhitespace() {
  if (IsEmpty())
    return;

  auto till = std::find_if(cur_line_it_ + cur_reading_pos_,
                           cur_line_end_it_, sim_string_utils::is_space);
  if (till != cur_line_end_it_)
    cur_reading_pos_ = std::distance(cur_line_it_, till);
}

std::string LineReader::ExtractAndSubstrUntil(
    std::function<bool(unsigned char)> &predicate) {
  std::stringstream extract_builder;
  while (!IsEmpty()) {
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
  if (IsEmpty()) {
    return false;
  }

  auto begin = cur_line_it_ + cur_reading_pos_;
  auto end = std::find_if(begin, cur_line_end_it_, predicate);

  if (end != cur_line_end_it_) {
    auto distance = std::distance(begin, end);
    cur_reading_pos_ += distance;
    return true;
  }

  return false;
 }

#if 0
bool LineReader::TrimTillConsume(const std::string &to_consume, bool strict) {
  if (IsEmpty() || CurLength() < to_consume.length()) {
    return false;
  }

  auto sub_start = cur_line_.begin() + cur_reading_pos_;
  auto start = std::search(sub_start, cur_line_.end(), to_consume.begin(), to_consume.end());
  if (start == cur_line_.end() || (strict && start != sub_start)) {
    return false;
  }

  cur_reading_pos_ = std::distance(cur_line_.begin(), start + to_consume.length());
  return true;
}
#endif

bool LineReader::ConsumeAndTrimTillString(const std::string &to_consume) {
  if (IsEmpty() || CurLength() < to_consume.length()) {
    return false;
  }

  auto sub_start = cur_line_it_ + cur_reading_pos_;
  auto sub_end = cur_line_end_it_;
  auto tf_start = to_consume.begin();
  auto tf_end = to_consume.end();

  size_t consumed = 0;
  const size_t to_match = to_consume.length();
  size_t matched;
  while(sub_start != sub_end and tf_start != tf_end) {
    // search for potential start
    for (; *tf_start != *sub_start and sub_start != sub_end; sub_start++, consumed++)
      ;
    if (sub_start == sub_end) {
      return false;
    }

    // try matching
    matched = 0;
    for (; tf_start != tf_end; ++tf_start, ++sub_start, ++consumed) {
      if (sub_start == sub_end) {
        return false;
      }

      const char trie = *tf_start;
      const char match = *sub_start;
      if (trie != match) {
        break;
      }
      ++matched;
    }

    if (matched == to_match) {
      cur_reading_pos_ = cur_reading_pos_ + consumed;
      return true;
    }
    tf_start -= matched;
  }

  return false;
}

bool LineReader::ConsumeAndTrimString(const std::string &to_consume) {
  if (IsEmpty() || CurLength() < to_consume.length()) {
    return false;
  }

  auto sub_start = cur_line_it_ + cur_reading_pos_;
  auto tf_start = to_consume.begin();
  auto tf_end = to_consume.end();

  size_t consumed = 0;
  while (tf_start != tf_end) {
    const char to_match = *tf_start;
    const char match = *sub_start;
    if (to_match != match) {
      return false;
    }
    ++sub_start;
    ++tf_start;
    ++consumed;
  }

  cur_reading_pos_ = cur_reading_pos_ + consumed;
  return true;
}

bool LineReader::ConsumeAndTrimChar(const char to_consume) {
  if (IsEmpty()) {
    return false;
  }
  const char letter = cur_line_[cur_reading_pos_];
  if (letter != to_consume)
    return false;

  ++cur_reading_pos_;
  return true;
}

bool LineReader::ParseUintTrim(int base, uint64_t &target) {
  if (IsEmpty() or (base != 10 and base != 16)) {
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
  if (IsEmpty()) {
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
