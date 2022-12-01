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

#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <fstream>
#include <iostream>

#include "lib/utils/log.h"

namespace reader {
class LineReader {
  const std::string &file_path_;
  std::istream *input_stream_;
  int line_number_ = 0;

  /* for gz files */
  std::ifstream *gz_file_;
  boost::iostreams::filtering_istreambuf *gz_is_buf_;

  explicit LineReader(const std::string &file_path, std::istream *input_stream)
      : file_path_(file_path), input_stream_(input_stream) {
  }

  explicit LineReader(const std::string &file_path, std::istream *input_stream,
                      std::ifstream *gz_file,
                      boost::iostreams::filtering_istreambuf *gz_is_buf)
      : file_path_(file_path),
        input_stream_(input_stream),
        gz_file_(gz_file),
        gz_is_buf_(gz_is_buf) {
  }

 public:
  ~LineReader() {
    delete input_stream_;
    if (gz_file_ != nullptr)
      delete gz_file_;

    if (gz_is_buf_ != nullptr)
      delete gz_is_buf_;
  }

  inline static LineReader create(const std::string &file_path) {
    std::istream *i_stream =
        new std::ifstream(file_path, std::ios_base::in | std::ios_base::binary);
    return LineReader(file_path, i_stream);
  }

  inline static LineReader create_gz(const std::string &file_path) {
    std::ifstream *gz_file =
        new std::ifstream(file_path, std::ios_base::in | std::ios_base::binary);
    boost::iostreams::filtering_streambuf<boost::iostreams::input> *gz_in =
        new boost::iostreams::filtering_streambuf<boost::iostreams::input>();
    gz_in->push(boost::iostreams::gzip_decompressor());
    gz_in->push(*gz_file);
    std::istream *i_stream = new std::istream(gz_in);
    return LineReader(file_path, i_stream, gz_file, gz_in);
  }

  int ln() {
    return line_number_;
  }

  bool is_valid() {
    return input_stream_->good();
  }

  bool get_next_line(std::string &target, bool skip_empty_line) {
    if (!is_valid()) {
      DFLOGERR("could not open file '%s'", file_path_);
      return false;
    }

    while (true) {
      if (!std::getline(*input_stream_, target)) {
        DLOGIN("reached end of file");
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
};

}  // namespace reader

#endif  // SIMBRICKS_READER_H_
