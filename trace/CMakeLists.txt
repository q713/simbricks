#
# Copyright 2022 Max Planck Institute for Software Systems, and
# National University of Singapore
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.18)

project(simbricks-trace LANGUAGES CXX DESCRIPTION "simbricks trace utility for analyzing system performance in depth")

# if not specified otherwise build in release mode
# can be spedcified by -DCMAKE_BUILD_TYPE=Debug or -DCMAKE_BUILD_TYPE=Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# we require c++20 --> coroutines
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# compiler flags for either debug or release mode
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# all src files required to build the trace functionality
set(TRACE_SRC_FILES

        trace.cc

        util/exception.h
        util/componenttable.h
        util/cxxopts.hpp
        util/log.h

        corobelt/corobelt.h

        reader/reader.h
        reader/reader.cc

        parser/parser.h
        parser/parser.cc
        parser/nicbm.cc
        parser/gem5.cc

        events/events.h
        events/events.cc
        events/event-filter.h
        events/eventStreamParser.h

        env/stringInternalizer.h
        env/symtable.h
        env/symtable.cc
        env/traceEnvironment.h
        env/traceEnvironment.cc

        analytics/span.h
        analytics/trace.h
        analytics/tracer.h
        analytics/queue.h
        analytics/spanner.h
        analytics/nicSpanner.cc
        analytics/hostSpanner.cc
        )

add_executable(${PROJECT_NAME} ${TRACE_SRC_FILES})

target_include_directories( ${PROJECT_NAME} PUBLIC
        util
        reader
        env
        corobelt
        events
        parser
        analytics
        )

# Check whether the correct compiler is used
message(STATUS "The used compiler id: ${CMAKE_CXX_COMPILER_ID}")
# only gcc is supported at the moment
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    message(FATAL "GNU compiler are currently not supported")
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PUBLIC -pthread)
else()
    message(FATAL_ERROR "Your compiler, whatever it is, is currently not supported.")
endif()

# link coroutine library
add_subdirectory(libs/concurrencpp)
target_link_libraries(${PROJECT_NAME} PUBLIC concurrencpp::concurrencpp)

set(TEST_SRC
        util/exception.h
        corobelt/corobelt.h
        corobelt/concurrencpp-test.cpp
        )

set(TEST_TAR concurrencpp-test)
add_executable(${TEST_TAR} ${TEST_SRC})

target_include_directories(${TEST_TAR} PUBLIC
        util
        corobelt
        )

target_link_libraries(${TEST_TAR} PUBLIC concurrencpp::concurrencpp)

