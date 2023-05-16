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
#include "corobelt/corobelt.h"

TEST_CASE("Test coroutine channel", "[Channel]") {
  auto concurren_options = concurrencpp::runtime_options();
  concurren_options.max_background_threads = 0;
  concurren_options.max_cpu_threads = 1;
  const concurrencpp::runtime runtime{concurren_options};
  const auto thread_pool_executor = runtime.thread_pool_executor();

  Channel<int, 3> channel_to_test;

  SECTION("can push into channel") {
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 1).get());
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 2).get());
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 3).get());

    REQUIRE_FALSE(channel_to_test.try_push_non_lazy(thread_pool_executor, 4).get());
  }

  SECTION("channel does not change order") {
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 1).get());
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 2).get());
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 3).get());

    REQUIRE(channel_to_test.pop_non_lazy(thread_pool_executor).get().value_or(-1) == 1);
    REQUIRE(channel_to_test.pop_non_lazy(thread_pool_executor).get().value_or(-1) == 2);
    REQUIRE(channel_to_test.pop_non_lazy(thread_pool_executor).get().value_or(-1) == 3);
  }

  SECTION("cannot pull from empty channel") {
    REQUIRE_FALSE(channel_to_test.try_pop_non_lazy(thread_pool_executor).get());
  }

  SECTION("can read from and not write to closed channel") {
    REQUIRE(channel_to_test.push_non_lazy(thread_pool_executor, 1).get());

    channel_to_test.close_channel(thread_pool_executor).get();

    REQUIRE(channel_to_test.pop_non_lazy(thread_pool_executor).get().value_or(-1) == 1);
    REQUIRE_FALSE(channel_to_test.try_push_non_lazy(thread_pool_executor, 2));
  }
}
