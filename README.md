# Zth (libzth) - Zeta threads

This library provides user-space cooperative multitasking, also known as
fibers. One can see fibers as threads, but with the exception that you
explicitly have to indicate when the fiber is allowed to switch to another
fiber. As a result, locking, synchronization, using shared data structures
between fibers is way more easier than when using threads. See also
<https://en.wikipedia.org/wiki/Fiber_(computer_science)>.

Check out the `examples` to get a grasp about how it works.

A predecessor project was called Xi, as the Greek capital symbol suggests
parallel threads.  In this project, preemptive multitasking is implemented. In
the same context, the Z(eta) symbol suggests that threads are not parallel, but
they explicitly yield from one to another.

[GNU Pth](https://www.gnu.org/software/pth/) has been a great inspiration for this library.

Currently, Linux/Mac OSX/Windows, 32/64-bit, gcc >=4 is supported.
There is experimental support for bare-metal 32-bit ARM (with newlib 3.1.0).


## How to build

When starting from scratch, clone the repository, like:

	git clone https://github.com/jhrutgers/zth.git
	cd zth
	git submodule init
	git submodule update

Build Zth out-of-tree:

	mkdir build
	cd build
	cmake ..
	cmake --build .

After building, check out the `doc` directory for documentation.

For Mac OSX with MacPorts, one could change the sequence above by something like:

	CC=gcc-mp-8 CXX=g++-mp-8 LDFLAGS=-L/opt/local/lib cmake ..

By default, release builds are generated. To do debug builds, do something like:

	cmake .. -D CMAKE_BUILD_TYPE=Debug

For Windows with Qt, one could run cmake like this:

	set PATH=c:\Program Files\CMake\bin;%PATH%
	cmake.exe .. -D CMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" -D CMAKE_MAKE_PROGRAM="c:/qt/Tools/mingw730_64/bin/mingw32-make.exe" -D CMAKE_C_COMPILER="c:/qt/Tools/mingw730_64/bin/gcc.exe" -D CMAKE_CXX_COMPILER="c:/qt/Tools/mingw730_64/bin/g++.exe"


## How to run

libzth checks for environment variables, which are listed below.  The
environment is (usually) only checked once, so dynamically changing the
variables after startup has no effect to libzth's behavior.

* `ZTH_CONFIG_ENABLE_DEBUG_PRINT`  
	When set to 0, debug prints are suppressed.  Enabled by default. For
	non-debug builds, all debug prints are removed from the binary, so they
	cannot be enabled in that case.

* `ZTH_CONFIG_DO_PERF_EVENT`  
	When set to 1, perf VCD file generation is enabeled.  Disabled by default.

* `ZTH_CONFIG_DO_PERF_SYSCALL`  
	When set to 1, the perf VCD logs will contain all calls to Zth's special
	functions.  Enabled by default.

* `ZTH_CONFIG_CHECK_TIMESLICE_OVERRUN`
	When set to 1, check at every context switch if the timeslice was overrun
	significantly.  Only the longest overrun is reported.  Enabled by default
	in the debug build, not available in release builds.


## License

The project license is specified in COPYING and COPYING.LESSER.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

