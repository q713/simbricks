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

#ifndef SIMBRICKS_READER_H_
#define SIMBRICKS_READER_H_

#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include "util/log.h"
#include "util/string_util.h"

class LineReader {
  std::ifstream input_stream_;
  size_t line_number_ = 0;
  size_t cur_reading_pos_ = 0;
  std::string::iterator cur_line_it_;
  std::string::iterator cur_line_end_it_;
  std::string cur_line_;
  bool skip_empty_lines_ = true;

 public:
  explicit LineReader() = default;

  explicit LineReader(bool skip_empty_lines)
      : skip_empty_lines_(skip_empty_lines) {
  }

  void CloseInput() {
    if (input_stream_.is_open()) {
      input_stream_.close();
    }
  }

  ~LineReader() {
    CloseInput();
  }

  inline size_t LinenNumber() const {
    return line_number_;
  }

  inline bool IsValid() {
    return input_stream_.good();
  }

  std::string GetCurString() {
    if (cur_reading_pos_ >= cur_line_.length())
      return "";
    return std::string(cur_line_.begin() + cur_reading_pos_, cur_line_.end());
  }

  inline const std::string &GetRawLine() {
    return cur_line_;
  }

  inline size_t CurLength() {
    return cur_line_.length() - cur_reading_pos_;
  }

  inline bool IsEmpty() {
    return CurLength() <= 0;
  }

  bool OpenFile(const std::string &file_path);

  bool NextLine();

  bool MoveForward(size_t steps);

  void TrimL();

  void TrimTillWhitespace();

  std::string ExtractAndSubstrUntil(
      std::function<bool(unsigned char)> &predicate);

  bool ExtractAndSubstrUntilInto(
      std::string &target, std::function<bool(unsigned char)> &predicate) {
    target = ExtractAndSubstrUntil(predicate);
    return not target.empty();
  }

  bool SkipTill(std::function<bool(unsigned char)> &predicate);

  bool SkipTillWhitespace() {
    return SkipTill(sim_string_utils::is_space);
  }

  //bool TrimTillConsume(const std::string &to_consume, bool strict);

  bool ConsumeAndTrimTillString(const std::string &to_consume);

  bool ConsumeAndTrimString(const std::string &to_consume);

  bool ConsumeAndTrimChar(char to_consume);

  bool ParseUintTrim(int base, uint64_t &target);

  bool ParseInt(int &target);
};

/*
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

class GzLineReader : public Reader {
  std::ifstream gz_file_;
  boost::iostreams::filtering_streambuf<boost::iostreams::input> gz_in_;
  std::istream *input_stream_ = nullptr;
  int line_number_ = 0;


 public:
  explicit GzLineReader(const std::string &file_path)
      : Reader(file_path),
        gz_file_(std::move(std::ifstream(
            file_path, std::ios_base::in | std::ios_base::binary))) {
    gz_in_.push(boost::iostreams::gzip_decompressor());
    gz_in_.push(gz_file_);
    input_stream_ = new std::istream(&gz_in_);
  }

  ~GzLineReader() {
    if (input_stream_) {
      delete input_stream_;
    }
  }

  bool IsValid() override {
    return input_stream_ != nullptr && input_stream_->good();
  }

  bool get_next_line(std::string &target, bool skip_empty_line) override {
    return Reader::get_next_line(*input_stream_, target, skip_empty_line);
  }
};
*/

#endif  // SIMBRICKS_READER_H_
