# Copyright 2021 Max Planck Institute for Software Systems, and
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

include mk/subdir_pre.mk

#bin_trace_process := $(d)process
#OBJS := $(addprefix $(d), process.o sym_map.o log_parser.o gem5.o nicbm.o)
#$(bin_trace_process): $(OBJS) -lboost_iostreams -lboost_coroutine \
#	-lboost_context

bin_trace := $(d)trace
OBJS_TRACE := $(addprefix $(d), trace.o filter/symtable.o parser/parser.o)
$(bin_trace): $(OBJS_TRACE) -lboost_coroutine

CLEAN := $(bin_trace) $(OBJS_TRACE) #$(bin_trace_process) $(OBJS)
ALL := $(bin_trace) # $(bin_trace_process)

include mk/subdir_post.mk
