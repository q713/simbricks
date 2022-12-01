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

#include <fstream>

#include "lib/utils/log.h"

class LineReader { // TODO: add support for gz files
  const std::string &file_path_;
  std::ifstream input_stream_;
  int line_number_ = 0;

  public:
    LineReader(const std::string &file_path) : file_path_(file_path), 
      input_stream_(std::ifstream(file_path, std::ios_base::in | std::ios_base::binary)) {}

    ~LineReader() {
      input_stream_.close();
    }

    int ln() {
      return line_number_;
    }

    bool is_valid() {
      return input_stream_.is_open();
    }

    bool get_next_line(std::string &target, bool skip_empty_line) {
      if (!is_valid()) {
        DFLOGERR("could not open file '%s'", file_path_);
        return false;
      }

      while (true) {
        if (!std::getline(input_stream_, target)) {
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

#endif // SIMBRICKS_READER_H_
