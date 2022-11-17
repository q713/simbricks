#ifndef SIMBRICKS_LOG_H_
#define SIMBRICKS_LOG_H_

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>

namespace sim_log {

#define SIMLOG 1

enum StdTarget { to_err, to_out, to_file };
class Log {
 private:
  Log(FILE *file, StdTarget target) : file_(file), target_(target) {
    file_mutex_ = new std::mutex{};
  }

  // TODO: use ostream..., no simple format without c++20...
 public:
  std::mutex *file_mutex_;
  FILE *file_;
  StdTarget target_;

  ~Log() {
    if (file_mutex_ != nullptr) {
      delete file_mutex_;
    }

    if (file_ != nullptr) {
      fclose(file_);
    }
  }

  static const Log *createLog(StdTarget target) {
    if (target == StdTarget::to_out) {
      return new Log(stdout, StdTarget::to_out);
    } else {
      return new Log(stderr, StdTarget::to_err);
    } 
  }

  static const Log *createLog(const char *file_path) {
    if (file_path == nullptr)
      return nullptr;

    FILE* file = fopen(file_path, "w");
    if (file == nullptr)
      return nullptr;

    return new Log(file, StdTarget::to_file);
  }
};

class Logger {
 private:
  const std::string &prefix_;

  template <typename... Args>
  inline void log_internal(FILE *out, const char *format, Args... args) {
    fprintf(out, prefix_.c_str());
    fprintf(out, format, args...);
  }

  inline void log_internal(FILE *out, const char *to_print) {
    fprintf(out, prefix_.c_str());
    fprintf(out, to_print);
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

  template <typename... Args>
  inline void log_stdout_f(const char *format, const Args &...args) {
    log_internal(stdout, format, args...);
  }

  template <typename... Args>
  inline void log_stderr_f(const char *format, const Args &...args) {
    log_internal(stderr, format, args...);
  }

  inline void log_stdout(const char *to_print) {
    log_internal(stdout, to_print);
  }

  inline void log_stderr(const char *to_print) {
    log_internal(stderr, to_print);
  }

  template <typename... Args>
  void log_file_f(const Log &log, const char *format, const Args &...args) {
    std::lock_guard<std::mutex> guard(*(log.file_mutex_));

    log_internal(log.file_, format, args...);
  }

  void log_file(const Log &log, const char *to_print) {
    std::lock_guard<std::mutex> guard(*(log.file_mutex_));

    log_internal(log.file_, to_print);
  }

  template <typename... Args>
  void log_f(const Log &log, const char *format, const Args &...args) {
    if (log.target_ == StdTarget::to_file) {
      log_file_f(log, format, args...);
    } else if (log.target_ == StdTarget::to_out) {
      log_stdout_f(format, args...);
    } else {
      log_stderr_f(format, args...);
    }
  }

  void log(const Log &log, const char *to_print) {
    if (log.target_ == StdTarget::to_file) {
      log_file(log, to_print);
    } else if (log.target_ == StdTarget::to_out) {
      log_stdout(to_print);
    } else {
      log_stderr(to_print);
    }
  }
};

#ifdef SIMLOG

#define DFLOGINFLOG(l, fmt, ...) \
  sim_log::Logger::getInfoLogger().log_f(l, fmt, __VA_ARGS__);

#define DFLOGWARNLOG(l, fmt, ...) \
  sim_log::Logger::getWarnLogger().log_f(l, fmt, __VA_ARGS__);

#define DFLOGERRLOG(l, fmt, ...) \
  sim_log::Logger::getErrorLogger().log_f(l, fmt, __VA_ARGS__);

#define DFLOGIN(fmt, ...) \
  sim_log::Logger::getInfoLogger().log_stdout_f(fmt, __VA_ARGS__);

#define DFLOGWARN(fmt, ...) \
  sim_log::Logger::getWarnLogger().log_stderr_f(fmt, __VA_ARGS__);

#define DFLOGERR(fmt, ...) \
  sim_log::Logger::getErrorLogger().log_stderr_f(fmt, __VA_ARGS__);

#define DLOGINFLOG(l, tp) sim_log::Logger::getInfoLogger().log(l, tp);

#define DLOGWARNLOG(l, tp) sim_log::Logger::getWarnLogger().log(l, tp);

#define DLOGERRLOG(l, tp) sim_log::Logger::getErrorLogger().log(l, tp);

#define DLOGIN(tp) sim_log::Logger::getInfoLogger().log_stdout(tp);

#define DLOGWARN(tp) sim_log::Logger::getWarnLogger().log_stderr(tp);

#define DLOGERR(tp) sim_log::Logger::getErrorLogger().log_stderr(tp);

#else

#define DFLOGINFLOG(l, fmt, ...) \
  do {                           \
  } while (0)

#define DFLOGWARNLOG(l, fmt, ...) \
  do {                            \
  } while (0)

#define DFLOGERRLOG(l, fmt, ...) \
  do {                           \
  } while (0)

#define DFLOGIN(fmt, ...) \
  do {                    \
  } while (0)

#define DFLOGWARN(fmt, ...) \
  do {                      \
  } while (0)

#define DFLOGERR(fmt, ...) \
  do {                     \
  } while (0)

#define DLOGINFLOG(l, tp) \
  do {                    \
  } while (0)

#define DLOGWARNLOG(l, tp) \
  do {                     \
  } while (0)

#define DLOGERRLOG(l, tp) \
  do {                    \
  } while (0)

#define DLOGIN(tp) \
  do {             \
  } while (0)

#define DLOGWARN(tp) \
  do {               \
  } while (0)

#define DLOGERR(tp) \
  do {              \
  } while (0)

#endif

}  // namespace sim_log

#endif  // SIMBRICKS_LOG_H_
