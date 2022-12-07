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

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>

#include "lib/utils/log.h"

class Reader {
  int line_number_ = 0;

 protected:
  const std::string &file_path_;
  virtual bool is_valid() = 0;

  bool get_next_line(std::istream &in, std::string &target,
                     bool skip_empty_line) {
    if (!is_valid()) {
      DFLOGERR("could not open file '%s'", file_path_.c_str());
      return false;
    }

    while (true) {
      if (!std::getline(in, target)) {
        return false;
      }
      line_number_++;

      if (skip_empty_line && target.empty()) {
        continue;
      }

      break;
    }

    return true;
  }

 public:
  explicit Reader(const std::string &file_path) : file_path_(file_path) {
  }

  int ln() {
    return line_number_;
  }

  virtual bool get_next_line(std::string &target, bool skip_empty_line) = 0;
};

class LineReader : public Reader {
  std::ifstream input_stream_;

 public:
  explicit LineReader(const std::string &file_path)
      : Reader(file_path),
        input_stream_(std::move(std::ifstream(
            file_path, std::ios_base::in | std::ios_base::binary))) {
  }

  bool is_valid() override {
    return input_stream_.is_open();
  }

  bool get_next_line(std::string &target, bool skip_empty_line) override {
    return Reader::get_next_line(input_stream_, target, skip_empty_line);
  }

  static std::optional<LineReader> create(const std::string &file_path) {
    LineReader line_reader(file_path);
    if (!line_reader.is_valid()) {
      DFLOGERR("could not open file with path '%s'\n", file_path.c_str());
      return std::nullopt;
    }
    return line_reader;
  }
};

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

  bool is_valid() override {
    return input_stream_ != nullptr && input_stream_->good();
  }

  bool get_next_line(std::string &target, bool skip_empty_line) override {
    return Reader::get_next_line(*input_stream_, target, skip_empty_line);
  }
};

#endif  // SIMBRICKS_READER_H_
