#ifndef SIMBRICKS_LOG_H_
#define SIMBRICKS_LOG_H_

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

namespace sim_log {

enum class StdTarget {
  to_err,
  to_out,
  to_file
};           // TODO: remove this by inheritance and streams
class Log {  // TODO: use ostream..., no simple format without c++20...
 public:
  std::mutex *file_mutex_;
  const std::string &file_;
  StdTarget target_;

  Log() : target_(StdTarget::to_err), file_("") {
  }

  Log(const std::string &file_path)
      : file_(file_path), target_(StdTarget::to_file) {
    file_mutex_ = new std::mutex{};
  }

  ~Log() {
    if (target_ == StdTarget::to_file) {
      delete file_mutex_;
    }
  }
};

class Logger {
 private:
  const std::string &prefix_;

  template <typename ...Args>
  inline void log_internal(FILE *out, const char *format, Args... args) {
    fprintf(out, prefix_.c_str());
    fprintf(out, format, args...);
  }

 public:
  Logger(const std::string &prefix) : prefix_(prefix) {
  }

  static Logger &getInfoLogger() {
    static Logger logger("info: ");
    return logger;
  }

  static Logger &getErrorLogger() {
    static Logger logger("error: ");
    return logger;
  }

  static Logger &getWarnLogger() {
    static Logger logger("warn: ");
    return logger;
  }

  template <typename ...Args>
  void log_file(const Log &log, const char *format, const Args &...args) {
    std::lock_guard<std::mutex> guard(*(log.file_mutex_));

    // TODO: do not always open the file..., streams...
    FILE *out = fopen(log.file_.c_str(), "a");
    if (out == nullptr) {
      log_stderr(format, args...);
    }

    log_internal(out, format, args...);

    fclose(out);
  }

  template <typename ...Args>
  inline void log_stdout(const char *format, const Args &...args) {
    log_internal(stdout, format, args...);
  }

  template <typename ...Args>
  inline void log_stderr(const char *format, const Args &...args) {
    log_internal(stderr, format, args...);
  }

  template <typename ...Args>
  void log(const Log &log, const char *format, const Args &...args) {
    if (log.target_ == StdTarget::to_file) {
      log_file(log, format, args...);
    } else if (log.target_ == StdTarget::to_out) {
      log_stdout(format, args...);
    } else {
      log_stderr(format, args...);
    }
  }
};

#define DLOGINFLOG(l, fmt, ...) \
  sim_log::Logger::getInfoLogger().log(l, fmt, __VA_ARGS__);

#define DLOGWARNLOG(l, fmt, ...) \
  sim_log::Logger::getWarnLogger().log(l, fmt, __VA_ARGS__);

#define DLOGERRLOG(l, fmt, ...) \
  sim_log::Logger::getErrorLogger().log(l, fmt, __VA_ARGS__);

#define DLOGIN(fmt, ...) \
  sim_log::Logger::getInfoLogger().log_stdout(fmt, __VA_ARGS__);

#define DLOGWARN(fmt, ...) \
  sim_log::Logger::getWarnLogger().log_stderr(fmt, __VA_ARGS__);

#define DLOGERR(fmt, ...) \
  sim_log::Logger::getErrorLogger().log_stderr(fmt, __VA_ARGS__);

}  // namespace sim_log

#endif  // SIMBRICKS_LOG_H_