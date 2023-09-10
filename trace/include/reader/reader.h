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

#include <fcntl.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include "sync/corobelt.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/utils.h"

class LineHandler {
  size_t cur_reading_pos_ = 0;
  std::string::iterator cur_line_it_;
  std::string::iterator cur_line_end_it_;
  std::string cur_line_;

 public:
  explicit LineHandler() = default;

  inline void SetLine(std::string &&line) {
    cur_line_ = line;
    ResetPos();
  }

  inline void SetLine(const std::string &line) {
    cur_line_ = line;
    ResetPos();
  }

  // Use with care!
  inline std::string &GetRawLineMut() {
    return cur_line_;
  }

  inline const std::string &GetRawLine() {
    return cur_line_;
  }

  inline void ResetPos() {
    cur_reading_pos_ = 0;
    cur_line_it_ = cur_line_.begin();
    cur_line_end_it_ = cur_line_.end();
  }

  inline std::string GetCurString() {
    if (cur_reading_pos_ >= cur_line_.length()) {
      return "";
    }
    return std::string(cur_line_.begin() + cur_reading_pos_, cur_line_.end());
  }

  inline size_t CurLength() {
    return cur_line_.length() - cur_reading_pos_;
  }

  inline bool IsEmpty() {
    return CurLength() <= 0;
  }

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

  bool ConsumeAndTrimTillString(const std::string &to_consume);

  bool ConsumeAndTrimString(const std::string &to_consume);

  bool ConsumeAndTrimChar(char to_consume);

  bool ParseUintTrim(int base, uint64_t &target);

  bool ParseInt(int &target);

  bool ParseBoolFromUint(int base, bool &target);

  bool ParseBoolFromStringRepr(bool &target);

  bool ParseBoolFromInt(bool &target);
};

template<size_t BufferSize> requires SizeLagerZero<BufferSize>
class ReaderBuffer {
  const std::string name_;
  const bool skip_empty_lines_;
  std::ifstream input_stream_;
  constexpr static size_t kPageSize = 4096;
  constexpr static size_t kBufSize = kPageSize * 64;//* 256;
  char *istream_buffer_ = nullptr;
  FILE *file_ = nullptr;
  // TODO: introduce maximum size for strings a.k.a lines --> then use arena allocation to allocate these strings
  std::vector<LineHandler> buffer_{BufferSize};
  size_t cur_size_ = 0;
  size_t cur_line_index_ = 0;

  LineHandler *GetHandler() {
    assert(cur_size_ > 0 and cur_line_index_ < cur_size_);
    LineHandler *handler = &buffer_[cur_line_index_];
    assert(handler);
    ++cur_line_index_;
    return handler;
  }

  bool IsStreamStillGood() const {
    return not input_stream_.eof() and input_stream_.good();
  }

  bool StillBuffered() const {
    return cur_size_ > 0 and cur_line_index_ < cur_size_;
  }

  void FillBuffer() {
    assert(cur_line_index_ == 0 or cur_line_index_ == cur_size_);

    size_t tries = 1;
    cur_size_ = 0;
    size_t index = 0;
    while (index < BufferSize) {
      if (not IsStreamStillGood()) {
        if (tries > 0) {
          std::cout << name_ << " try to read later more" << '\n';
          std::this_thread::sleep_for(std::chrono::seconds{3});
          --tries;
          continue;
        }
        break;
      }

      std::string &buf = buffer_[index].GetRawLineMut();
      std::getline(input_stream_, buf);

      if (input_stream_.fail()) {
        if (tries > 0) {
          std::cout << name_ << " try to read later more" << '\n';
          std::this_thread::sleep_for(std::chrono::seconds{3});
          --tries;
          continue;
        }
        break;
      }

      if (skip_empty_lines_ and buf.empty()) {
        continue;
      }

      buffer_[index].ResetPos();
      ++index;
      ++cur_size_;
    }
    cur_line_index_ = 0;
  }

  void Close() {
    if (file_) {
      fclose(file_);
    }
    if (istream_buffer_) {
      delete[] istream_buffer_;
    }
    if (input_stream_.is_open()) {
      input_stream_.close();
    }
  }

 public:
  bool HasStillLine() {
    return StillBuffered() or IsStreamStillGood();
  }

  std::pair<bool, LineHandler *> NextHandler() {
    if (not HasStillLine()) {
      return std::make_pair(false, buffer_.data());
    }

    if (not StillBuffered()) {
      FillBuffer();
      // maybe we have nothing buffered but the stream still appears to be good
      if (not StillBuffered()) {
        return std::make_pair(false, buffer_.data());
      }
    }

    assert(StillBuffered());
    return std::make_pair(true, GetHandler());
  }

  explicit ReaderBuffer(std::string name, const bool skip_empty_lines)
      : name_(std::move(name)), skip_empty_lines_(skip_empty_lines) {
  }

  void OpenFile(const std::string &file_path, bool is_named_pipe = false) {
    if (!std::filesystem::exists(file_path)) {
      throw_just(std::source_location::current(),
                 "ReaderBuffer: the file path'", file_path, "' does not exist");
    }
    throw_on(input_stream_.is_open(), "ReaderBuffer:OpenFile: already opened file to read");
    throw_on(file_, "ReaderBuffer:OpenFile: already opened file to read");

    if (is_named_pipe) {
      file_ = fopen(file_path.c_str(), "r");
      throw_if_empty(file_, "ReaderBuffer: could not open file path");
      const int fd = fileno(file_);
      throw_on(fd == -1, "ReaderBuffer: could not obtain fd");
      const int suc = fcntl(fd, F_SETPIPE_SZ, kBufSize);
      if (suc != kBufSize) {
        std::cout << "ReaderBuffer: could not change '" << file_path << "' size to " << kBufSize << '\n';
      } else {
        std::cout << "ReaderBuffer: changed size successfully" << '\n';
      }
    }

    input_stream_ = std::ifstream(file_path);
    if (not input_stream_.is_open()) {
      throw_just(std::source_location::current(),
                 "ReaderBuffer: could not open file path'", file_path, "'");
    }
    if (not input_stream_.good()) {
      throw_just(std::source_location::current(),
                 "ReaderBuffer: the input stream of the file regarding file path'",
                 file_path, "' is not good");
    }
    istream_buffer_ = new char[kBufSize];
    throw_on(not istream_buffer_, "ReaderBuffer: could not create istream buffer");
    input_stream_.rdbuf()->pubsetbuf(istream_buffer_, kBufSize);
  }

  ~ReaderBuffer() {
    Close();
  };
};

#endif // SIMBRICKS_READER_H_
