#ifndef SIMBRICKS_LOG_H_
#define SIMBRICKS_LOG_H_

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>

namespace sim_log {

enum class StdTarget { to_err, to_out };
class Log {
  std::mutex *file_mutex_;
  bool is_file_ = false;
  std::ostream *out_;

 public:
  Log(StdTarget target) {
    if (target == StdTarget::to_err) {
      out_ = &std::cerr;
    } else {
      out_ = &std::cout;
    }
  }

  Log(const std::string &file_path) {
    std::ofstream *o = new std::ofstream{file_path};
    if (o != nullptr && !o->is_open()) {
      out_ = o;
      file_mutex_ = new std::mutex{};
      is_file_ = true;
    } else {
      out_ = &std::cout;
    }
  }

  ~Log() {
    if (is_file_) {
      delete out_;
      delete file_mutex_;
    }
  }
};

class Logger {
 private:
  const std::string &prefix_;

  Logger(const std::string &prefix) : prefix_(prefix) {
  }

 public:
  static const Logger &getInfoLogger() {
    static Logger logger("info: ");
    return logger;
  }

  static const Logger &getErrorLogger() {
    static Logger logger("error: ");
    return logger;
  }

  static const Logger &getWarnLogger() {
    static Logger logger("warn: ");
    return logger;
  }

  template <typename... Args>
  static void log(const Log &log, const char *format, Args... args) {
    if (log.is_file_)
      std::lock_guard<std::mutex> guard(log.file_mutex_);

    log.out_ << prefix_;
    fprintf(log.out_, format, args);
  }
};

}  // namespace sim_log

#endif  // SIMBRICKS_LOG_H_