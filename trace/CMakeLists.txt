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

#######################################
# Build Type, by default build Release
# can be spedcified by -DCMAKE_BUILD_TYPE=Debug or -DCMAKE_BUILD_TYPE=Release
#######################################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_BUILD_TYPE Debug)
    add_compile_options(-fno-omit-frame-pointer)
else()
    set(CMAKE_BUILD_TYPE Release)
    add_compile_options(-O2)
endif()


#######################################
# Required C standard
#######################################
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#######################################
# Catch 2
#######################################
Include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.3.2 # or a later release
    )
FetchContent_MakeAvailable(Catch2)


#######################################
# Concurrencpp
#######################################
add_subdirectory(libs/concurrencpp)


#######################################
# The Trace lib
#######################################
# all src files required to build the trace functionality
set(TRACE_LIB_HEADER_FILES
        #utils
        include/util/exception.h
        include/util/componenttable.h
        include/util/cxxopts.hpp
        include/util/log.h
        # coroutine wrappers
        include/corobelt/corobelt.h
        # reader
        include/reader/reader.h
        # parser
        include/parser/parser.h
        # events
        include/events/events.h
        include/events/event-filter.h
        include/events/eventStreamParser.h
        # tracing environment
        include/env/stringInternalizer.h
        include/env/symtable.h
        include/env/traceEnvironment.h
        # analytics
        include/analytics/span.h
        include/analytics/trace.h
        include/analytics/tracer.h
        include/analytics/queue.h
        include/analytics/spanner.h
        )
set(TRACE_LIB_SOURCE_FILES
        # reader
        source/reader/reader.cc
        # parser
        source/parser/parser.cc
        source/parser/nicbm.cc
        source/parser/gem5.cc
        # events
        source/events/events.cc
        # tracing environment
        source/env/symtable.cc
        source/env/traceEnvironment.cc
        # analytics
        source/analytics/nicSpanner.cc
        source/analytics/hostSpanner.cc
        )
add_library(${PROJECT_NAME} ${TRACE_LIB_HEADER_FILES} ${TRACE_LIB_SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC
        include
        source
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
target_link_libraries(${PROJECT_NAME} PUBLIC concurrencpp::concurrencpp)
target_link_libraries(${PROJECT_NAME} PUBLIC Catch2::Catch2WithMain)


#######################################
# Trace - Tests
#######################################
set(UNIT_TESTS_TARGET unittests)
set (TRACE_UNIT_TESTS_SRC_FILES
        tests/channel-test.cpp
        )
add_executable(${UNIT_TESTS_TARGET} ${TRACE_UNIT_TESTS_SRC_FILES})
target_include_directories(${UNIT_TESTS_TARGET} PUBLIC
        tests
        )
target_link_libraries(${UNIT_TESTS_TARGET} PUBLIC Catch2::Catch2WithMain)
target_link_libraries(${UNIT_TESTS_TARGET} PUBLIC ${PROJECT_NAME})


#######################################
# Concurrencpp test file to play around with coroutines
#######################################
set(TEST_SRC
        include/util/exception.h
        include/corobelt/corobelt.h
        source/corobelt/concurrencpp-test.cpp
        )
set(TEST_TAR concurrencpp-test)
add_executable(${TEST_TAR} ${TEST_SRC})
target_include_directories(${TEST_TAR} PUBLIC
        include
        source
        )
target_link_libraries(${TEST_TAR} PUBLIC concurrencpp::concurrencpp)


#######################################
# Trace executable
#######################################
set(TRACE_SRC_FILES
    trace.cc
)
add_executable(trace ${TRACE_SRC_FILES})
target_link_libraries(trace ${PROJECT_NAME})



