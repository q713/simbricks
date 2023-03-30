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

# help string is printed when scons -h is invoked
Help("""
Type: 'scons' to build the trace production program,
      'scons debug=1' to build the trace debug program without optimizations turned on
      'scons verbose_link=1' to link the trace program with verbose linker flags
      'scons use_gcc=1' to use g++ for building, the default is clang++ (the other flags also apply for this).
      'scons -c/--clean' to cleanup the trace build.
""")

# compiler flags + compiler dependend flags
cxx_flags = '-Wall -Wextra -Wno-unused-parameter -fPIC'

# define external libraries needed for the built
libraries = Split('')
linkcom = '$LINK -o $TARGET $LINKFLAGS $__RPATH $SOURCES $_LIBDIRFLAGS'
libpath = []

# linker debugging flags
link_flags = []
verbose_link = ARGUMENTS.get('verbose_link', 0)
if int(verbose_link):
    link_flags = ['-Xlinker', '--verbose']

# compiler dependent cli flags for coroutines
use_gcc = ARGUMENTS.get('use_gcc', 0)
if int(use_gcc):
    cxx = 'g++'
    cxx_flags += ' -std=gnu++20 -fcoroutines'
    linkcom += ' -Wl,--start-group $_LIBFLAGS -Wl,--end-group'
else:
    link_flags.append('-stdlib=libc++')
    cxx = 'clang++'
    cxx_flags += ' -std=c++20 -fcoroutines-ts -stdlib=libc++'

# create the construction environment
env = Environment(CXX=cxx, CXXFLAGS=cxx_flags, CPPPATH = ['.'])

# debug build without optimizations
debug = ARGUMENTS.get('debug', 0)
if not int(debug):
    env.Append(CXXFLAGS = ' -O3')

# all source files that shall/need to be compiled
src_files = ['trace/trace.cc', Glob('trace/events/*.cc'), Glob('trace/filter/*.cc'), 
    Glob('trace/parser/*.cc'), Glob('trace/reader/*.cc')] 

# the program that shall be built
env.Program(target='trace/trace', source=src_files, LIBS=libraries, LIBPATH=libpath, LINKFLAGS=link_flags, LINKCOM = linkcom)

env.Program(target='trace/corobelt/coro-test', source=['trace/corobelt/coro-test.cc'], LIBS=libraries, LIBPATH=libpath, LINKFLAGS=link_flags, LINKCOM = linkcom)
