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

cmake_minimum_required(VERSION 3.25)

project(simbricks-trace LANGUAGES CXX DESCRIPTION "simbricks trace utility for analyzing system performance in depth")

# if not specified otherwise build in release mode
# can be spedcified by -DCMAKE_BUILD_TYPE=Debug or -DCMAKE_BUILD_TYPE=Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# we require c++20 --> coroutines
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# compiler flags for either debug or release mode
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# all src files required to build the trace functionality
set(TRACE_SRC_FILES

    trace.cc

    corobelt/corobelt.h

    util/componenttable.h
    util/cxxopts.hpp
    util/log.h
    
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
    
    analytics/pack.h
    analytics/packer.h
    analytics/nicPacker.cc
    analytics/hostPacker.cc
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

# only gcc is supported at the moment
if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    # make sure that we build with necessary options for coroutines
    target_compile_options(${PROJECT_NAME} PUBLIC -std=gnu++20 -fcoroutines)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    message(FATAL_ERROR "Clang is currently not supported.")
endif()

# TODO: replace this by using a proper coroutine library
#       e.g. https://github.com/jbaldwin/libcoro
set(CORO_TEST_SRC
    corobelt/corobelt.h
    corobelt/corobelt-test.cc
)
add_executable(coro-test ${CORO_TEST_SRC})

target_include_directories(coro-test PUBLIC 
    corobelt
)



