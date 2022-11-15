#ifndef SIMBRICKS_LOG_H_
#define SIMBRICKS_LOG_H_

#include<iostream>
#include<fstream>

#include<stdio.h>

namespace sim_log {

class Logger {
    private:
        const std::string &_log_file;
        const std::string &_prefix;
        int _line;
        bool _must_delete = false;
        std::ostream *_out;

    public:
        Logger(const std::string &log_file, const std::string &prefix) 
            : _log_file(log_file), _prefix(prefix) {
            
            // try to log to specified file
            std::ofstream *o = new std::ofstream{log_file};
            if (o == nullptr || !o->is_open()) {
                _out = o;
                _must_delete = true;
            } else {
                // fall back to stdout
                _out = &std::cout;
            }
        }

        Logger(const std::string &prefix) : _prefix(prefix) {
            // log to stdout
            _out = &std::cout;
        }

        ~Logger() {
            if (_must_delete)
                delete _out;
        }

        template<typename... Args>
        void log(const char *format, Args... args) {
            _out << _prefix; 
            fprintf(_out, format, args);
        }
};

}

#endif // SIMBRICKS_LOG_H_