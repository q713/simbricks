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

#include "corobelt/corobelt.h"
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
    cur_line_it_ = cur_line_.begin();
    cur_line_end_it_ = cur_line_.end();
    cur_reading_pos_ = 0;
  }

  inline void SetLine(const std::string &line) {
    cur_line_ = line;
    cur_line_it_ = cur_line_.begin();
    cur_line_end_it_ = cur_line_.end();
    cur_reading_pos_ = 0;
  }

  inline const std::string &GetRawLine() {
    return cur_line_;
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
};

template<size_t BufferSize> requires SizeLagerZero<BufferSize>
class ReaderBuffer {
  const std::string name_;
  const bool skip_empty_lines_;
  std::ifstream input_stream_;
  constexpr static size_t kBufSize = 4096 * 64;
  char *istream_buffer_ = nullptr;
  FILE *file_ = nullptr;
  // TODO: introduce maximum size for strings a.k.a lines --> then use arena allocation to allocate these strings
  std::vector<LineHandler> buffer_{BufferSize};
  size_t cur_size_ = 0;
  size_t cur_line_index_ = 0;

  //const std::string &GetLine() {
  //  assert(cur_size_ > 0 and cur_line_index_ < cur_size_);
  //  const std::string &buffered_line = buffer_[cur_line_index_];
  //  ++cur_line_index_;
  //  return buffered_line;
  //}

  LineHandler &GetHandler() {
    assert(cur_size_ > 0 and cur_line_index_ < cur_size_);
    LineHandler &handler = buffer_[cur_line_index_];
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
    // std::cout << name_ << " BufferSize: " << BufferSize << std::endl;
    // std::cout << name_ << " start filling buffer" << std::endl;
    size_t index = 0;
    while (index < BufferSize) {
      if (not IsStreamStillGood()) {
        if (tries > 0) {
          std::cout << name_ << " try to read later more" << std::endl;
          std::this_thread::sleep_for(std::chrono::seconds{3});
          --tries;
          continue;
        }
        break;
      }

      // std::cout << "try to get next line: " << std::this_thread::get_id() <<
      // std::endl;
      std::string buf;
      std::getline(input_stream_, buf);

      //size_t k_buf_size = 256;
      //char line_buf[k_buf_size];
      //char* c_buf = line_buf;
      //assert(file_);
      //size_t line_size = getline(&c_buf, &k_buf_size, file_);
      //if (line_size < 1) {
      //  std::cout << "could not read line" << std::endl;
      //  break;
      //}
      //buf = c_buf;
      // std::cout << "got next line: " << std::this_thread::get_id() <<
      // std::endl;

      if (input_stream_.fail()) {
        if (tries > 0) {
          std::cout << name_ << " try to read later more" << std::endl;
          std::this_thread::sleep_for(std::chrono::seconds{3});
          --tries;
          continue;
        }
        break;
      }

      if (skip_empty_lines_ and buf.empty()) {
        continue;
      }

      buffer_[index].SetLine(std::move(buf));
      ++index;
      ++cur_size_;
    }
    // std::cout << name_ << " read " << cur_size_ << " many values into reader
    // buffer" << std::endl;
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

  // TODO: return pointer a.k.a avoid copies + implement iterator!!
  std::pair<bool, LineHandler> NextHandler() {
    if (not HasStillLine()) {
      return std::make_pair(false, buffer_[0]);
    }

    if (not StillBuffered()) {
      FillBuffer();
      // maybe we have nothing buffered but the stream still appears to be good
      if (not StillBuffered()) {
        return std::make_pair(false, buffer_[0]);
      }
    }

    assert(StillBuffered());
    //std::cout << "cur_size_: " << cur_size_ << ", cur_line_index_: " << cur_line_index_ << std::endl;
    return std::make_pair(true, GetHandler());
  }

  explicit ReaderBuffer(std::string name, const bool skip_empty_lines)
      : name_(name), skip_empty_lines_(skip_empty_lines) {
  }

  void OpenFile(const std::string &file_path, bool is_named_pipe = false) {
    if (!std::filesystem::exists(file_path)) {
      throw_just("ReaderBuffer: the file path'", file_path, "' does not exist");
    }
    throw_on(input_stream_.is_open(), "ReaderBuffer:OpenFile: already opened file to read");
    throw_on(file_, "ReaderBuffer:OpenFile: already opened file to read");

    if (is_named_pipe) {
      file_ = fopen(file_path.c_str(), "r");
      throw_if_empty(file_, "ReaderBuffer: could not open file path");
      const int fd = fileno(file_);
      throw_on(fd == -1, "ReaderBuffer: could not obtain fd");
      const int suc = fcntl(fd, F_SETPIPE_SZ, kBufSize);
      throw_on(suc != kBufSize, "ReaderBuffer: could not change the size of the named pipe");
      std::cout << file_path << ": increased named pipe size to " << suc << std::endl;
    }

    input_stream_ = std::ifstream(file_path);
    if (not input_stream_.is_open()) {
      throw_just("ReaderBuffer: could not open file path'", file_path, "'");
    }
    if (not input_stream_.good()) {
      throw_just(
          "ReaderBuffer: the input stream of the file regarding file path'",
          file_path, "' is not good");
    }
    istream_buffer_ = new char[kBufSize];
    throw_on(not istream_buffer_, "ReaderBuffer: could not create istream buffer");
    input_stream_.rdbuf()->pubsetbuf(istream_buffer_, kBufSize);
    std::cout << "ReaderBuffer::Open: opened file '" << file_path << "'" << std::endl;
  }

  ~ReaderBuffer() {
    Close();
  };
};

#if 0
class LineReader {
  using ExecutorT = std::shared_ptr<concurrencpp::thread_pool_executor>;
  using ExecutorTT = std::shared_ptr<concurrencpp::thread_executor>;

  std::string name_;
  ExecutorT background_executor_;
  ExecutorT foreground_executor_;
  // std::ifstream input_stream_;
  size_t line_number_ = 0;
  size_t cur_reading_pos_ = 0;
  std::string::iterator cur_line_it_;
  std::string::iterator cur_line_end_it_;
  ReaderBuffer<500'000> buffer_;
  std::string cur_line_;  // TODO: use a pointer
  bool skip_empty_lines_ = true;

 public:
  explicit LineReader(std::string &&name, ExecutorT background_executor,
                      ExecutorT foreground_executor)
      : name_(name),
        background_executor_(std::move(background_executor)),
        foreground_executor_(std::move(foreground_executor)),
        buffer_(name_, true) {
    throw_if_empty(background_executor_,
                   "LineReader: background executor empty");
    throw_if_empty(foreground_executor_,
                   "LineReader: foreground executor empty");
  };

  explicit LineReader(std::string &&name, ExecutorT background_executor,
                      ExecutorT foreground_executor, bool skip_empty_lines)
      : name_(name),
        background_executor_(std::move(background_executor)),
        foreground_executor_(std::move(foreground_executor)),
        skip_empty_lines_(skip_empty_lines),
        buffer_(name_, skip_empty_lines) {
    throw_if_empty(background_executor_,
                   "LineReader: background executor empty");
    throw_if_empty(foreground_executor_,
                   "LineReader: foreground executor empty");
  }

  // void CloseInput() {
  //   if (input_stream_.is_open()) {
  //     input_stream_.close();
  //   }
  // }

  ~LineReader() = default;
  // {
  // CloseInput();
  //}

  inline size_t LinenNumber() const {
    return line_number_;
  }

  // inline bool IsValid() {
  //   return input_stream_.good();
  // }

  inline std::string GetCurString() {
    if (cur_reading_pos_ >= cur_line_.length()) {
      return "";
    }
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

  concurrencpp::lazy_result<bool> NextLine();

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
};
#endif

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
