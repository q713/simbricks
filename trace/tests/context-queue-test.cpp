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
#include <memory>
#include "analytics/queue.h"
#include "analytics/span.h"

TEST_CASE("Test context queue", "[ContextQueue]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 1;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  uint64_t const spanner_a_id = 0;
  uint64_t const spanner_b_id = 1;
  uint64_t const spanner_c_id = 1;

  auto dummy_parent = std::make_shared<host_call_span>(0);

  ContextQueue<1> queue_to_test;

  SECTION("can only register two parties") {
    REQUIRE_NOTHROW(queue_to_test.register_spanner(spanner_a_id));
    REQUIRE_NOTHROW(queue_to_test.register_spanner(spanner_b_id));

    REQUIRE_THROWS(queue_to_test.register_spanner(spanner_c_id));
  }

  SECTION("cant register spanner twice") {
    REQUIRE_NOTHROW(queue_to_test.register_spanner(spanner_a_id));

    REQUIRE_THROWS(queue_to_test.register_spanner(spanner_a_id));
  }

  SECTION("cannot push in channel when not registered") {
    REQUIRE_THROWS(queue_to_test.push(thread_pool_executor, spanner_a_id, expectation::mmio, dummy_parent).get());
  }

  SECTION("can push and pull in rigth direction with capacity") {
    REQUIRE_NOTHROW(queue_to_test.register_spanner(spanner_a_id));
    REQUIRE_NOTHROW(queue_to_test.register_spanner(spanner_b_id));

    // a can push into channel
    REQUIRE(queue_to_test.push(thread_pool_executor, spanner_a_id, expectation::mmio, dummy_parent).get());

    // a cannot push into full channel
    REQUIRE_FALSE(queue_to_test.try_push(thread_pool_executor, spanner_a_id, expectation::mmio, dummy_parent).get());

    // a cannot read anything from channel yet
    REQUIRE(queue_to_test.try_poll(thread_pool_executor, spanner_a_id).get() == nullptr);

    // b can push to and read from the channel
    REQUIRE(queue_to_test.push(thread_pool_executor, spanner_b_id, expectation::dma, dummy_parent).get());
    auto con_from_a_to_b = queue_to_test.poll(thread_pool_executor, spanner_b_id).get();
    REQUIRE(is_expectation(con_from_a_to_b, expectation::mmio));

    // a can read from channel
    auto con_from_b_to_a = queue_to_test.poll(thread_pool_executor, spanner_a_id).get();
    REQUIRE(is_expectation(con_from_b_to_a, expectation::dma));
  }
}