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

#ifndef SIMBRICKS_TRACE_PARSER_H_
#define SIMBRICKS_TRACE_PARSER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "util/exception.h"
#include "util/componenttable.h"
#include "corobelt/corobelt.h"
#include "events/events.h"
#include "reader/reader.h"
#include "env/traceEnvironment.h"


class LogParser : public producer<std::shared_ptr<Event>>
  {
  protected:
    const std::string name_;
    const uint64_t identifier_;
    const std::string log_file_path_;
    LineReader &line_reader_;

    bool parse_timestamp (uint64_t &timestamp);

    bool parse_address (uint64_t &address);

  public:
    explicit LogParser (const std::string name,
                        const std::string log_file_path,
                        LineReader &line_reader)
            : producer<std::shared_ptr<Event>> (),
              name_ (name),
              identifier_ (trace_environment::get_next_parser_id ()),
              log_file_path_ (log_file_path),
              line_reader_ (line_reader)
    {};

    inline uint64_t get_ident () const
    {
      return identifier_;
    }

    inline std::string get_name ()
    {
      return name_;
    }

    virtual concurrencpp::result<void>
    produce (std::shared_ptr<concurrencpp::executor> resume_executor,
             std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override
    {
      co_return;
    }
  };

class Gem5Parser : public LogParser
  {
    ComponentFilter &component_table_;

  protected:
    std::shared_ptr<Event> parse_global_event (uint64_t timestamp);

    std::shared_ptr<Event> parse_system_switch_cpus (uint64_t timestamp);

    std::shared_ptr<Event> parse_system_pc_pci_host (uint64_t timestamp);

    std::shared_ptr<Event>
    parse_system_pc_pci_host_interface (uint64_t timestamp);

    std::shared_ptr<Event> parse_system_pc_simbricks (uint64_t timestamp);

    std::shared_ptr<Event> parse_simbricks_event (uint64_t timestamp);

  public:
    static auto create(const std::string name,
                       const std::string log_file_path,
                       ComponentFilter &component_table,
                       LineReader &line_reader) {
      auto parser = std::make_shared<Gem5Parser>(name, log_file_path, component_table, line_reader);
      throw_if_empty(parser, parser_is_null);
      return parser;
    }

    explicit Gem5Parser (const std::string name,
                         const std::string log_file_path,
                         ComponentFilter &component_table,
                         LineReader &line_reader)
            : LogParser (name, log_file_path,
                         line_reader),
              component_table_ (component_table)
    {
    }

    concurrencpp::result<void>
    produce (std::shared_ptr<concurrencpp::executor> resume_executor,
             std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override;
  };

class NicBmParser : public LogParser
  {
  protected:
    bool parse_off_len_val_comma (uint64_t &off, size_t &len, uint64_t &val);

    bool parse_op_addr_len_pending (uint64_t &op, uint64_t &addr, size_t &len,
                                    size_t &pending, bool with_pending);

    bool parse_mac_address (uint64_t &address);

    bool parse_sync_info (bool &sync_pcie, bool &sync_eth);

  public:
    static auto create(const std::string name, const std::string log_file_path, LineReader &line_reader) {
      auto parser = std::make_shared<NicBmParser>(name, log_file_path, line_reader);
      throw_if_empty(parser, parser_is_null);
      return parser;
    }

    explicit NicBmParser (const std::string name,
                          const std::string log_file_path,
                          LineReader &line_reader)
            : LogParser (name, log_file_path,
                         line_reader)
    {
    }

    concurrencpp::result<void>
    produce (std::shared_ptr<concurrencpp::executor> resume_executor,
             std::shared_ptr<Channel<std::shared_ptr<Event>>> &tar_chan) override;
  };

#endif  // SIMBRICKS_TRACE_PARSER_H_