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

#include <catch2/catch_all.hpp>
#include <vector>
#include <memory>

#include "corobelt/corobelt.h"
#include "util/componenttable.h"
#include "reader/reader.h"
#include "parser/parser.h"
#include "util/factory.h"
#include "events/events.h"
#include "util/exception.h"

class EventChecker : public consumer<std::shared_ptr<Event>> {

  const size_t to_macth_;
  std::vector<std::shared_ptr<Event>> expected_events_;

 public:

  explicit EventChecker(std::vector<std::shared_ptr<Event>> expected_events)
      : consumer<std::shared_ptr<Event>>(),
        to_macth_(expected_events.size()),
        expected_events_(std::move(expected_events)) {
  }

  concurrencpp::result<void> consume(
      std::shared_ptr<concurrencpp::executor> resume_executor,
      std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan
  ) override {
    std::shared_ptr<Event> event;
    std::optional<std::shared_ptr<Event>> msg;
    size_t cur_match = 0;
    for (msg = co_await src_chan->pop(resume_executor); msg.has_value();
         msg = co_await src_chan->pop(resume_executor)) {
      event = msg.value();
      REQUIRE(event != nullptr);
      if (cur_match < to_macth_) {
        REQUIRE(event->equal(*expected_events_[cur_match]));
      } else {
        // we got more events as expected
        REQUIRE(false);
      }
      ++cur_match;
    }
    REQUIRE(cur_match == to_macth_);
    co_return;
  }
};

TEST_CASE("Test gem5 parser produces expected event stream", "[Gem5Parser]") {

  const std::string test_file_path{"./tests/raw-logs/gem5-events-test.txt"};
  const std::string parser_name{"Gem5ClientParser"};

  ComponentFilter comp_filter_client("ComponentFilter-Server");
  LineReader client_lr;
  auto gem5 = create_shared<Gem5Parser>(parser_is_null, parser_name,
                                        test_file_path,
                                        comp_filter_client, client_lr);
  const std::vector<std::shared_ptr<Event>> to_match{
      std::make_shared<HostMmioR>(0x1869691991749, gem5->get_ident(), parser_name, 41735887303304840, 0xc0080300, 4, 0, 0x80300),
      std::make_shared<HostMmioR>(0x1869693118999, gem5->get_ident(), parser_name, 41735887303304840, 0xc0080300, 4, 0, 0x80300),
      std::make_shared<HostMmioR>(0x1869699347625, gem5->get_ident(), parser_name, 41735887311083304, 0xc040000c, 4, 3, 0xc),
      std::make_shared<HostMmioR>(0x1869699662249, gem5->get_ident(), parser_name, 41735887311083808, 0xc040001c, 4, 3, 0x1c)
  };
  auto checker = std::make_shared<EventChecker>(to_match);

  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 1;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();
  run_pipeline<std::shared_ptr<Event>>(thread_pool_executor, gem5, checker);

}


