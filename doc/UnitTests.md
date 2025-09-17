# Unit Testing in OAI

OpenAirInterface uses
[ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) for unit
testing. The cmake documentation has a
[tutorial](https://cmake.org/cmake/help/book/mastering-cmake/chapter/Testing%20With%20CMake%20and%20CTest.html)
explaining how to test with cmake and ctest; it is a suggested read, and the
following just lists the main points of how to compile the tests and how to add
new ones.

GoogleTest is a C++ unit testing framework that has been added as an external dependency. While using GoogleTest is not a requirement it can simplify writing unit tests.
See [primer](http://google.github.io/googletest/primer.html) for a quick introduction. To add it to your test executable link against
`GTest::gtest` or `GTest::gtest_main`.

# How to compile tests

To compile only the tests, a special target `tests` is available. It has to be
enabled with the special cmake variable `ENABLE_TESTS`:

```bash
cd openairinterface5g
mkdir build && cd build # you can also do that in cmake_targets/ran_build/build
cmake .. -GNinja -DENABLE_TESTS=ON
ninja tests
```

The user can use either `ninja` or `make`.

# Run unit tests

Then, you can run `ctest` to run all tests:

```bash
$ ctest
Test project /home/richie/w/ctest/build
    Start 1: nr_rlc_tests
1/1 Test #1: nr_rlc_tests .....................   Passed    0.06 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.06 sec
```

The user can see all available tests by typing `ctest -N` and then run a specific with `ctest -R <test_name>`, e.g. `ctest -R nr_rlc_tests`.

A couple of interesting variables are `--verbose`, `--output-on-failure`.

# How to add a new test

As of now, there is no dedicated testing directory. Rather, tests are together
with the sources of the corresponding (sub)system. The generic four-step
process is

1. Guard all the following steps with `if(ENABLE_TESTS)`. In a world where OAI
   is tested completely, there would be many executables that would be of
   tangential interest to the average user only. A "normal" build without tests
   would result in less executables, due to this guard.
2. Add an executable that you want to execute. In a `CMakeLists.txt`, do for
   instance `add_executable(my_test mytest.c)` where `mytest.c` contains
   `main()`. You can then build this executable with `ninja/make my_test`,
   given you ran `cmake -DENABLE_TESTS=ON ...` before.
3. Create a dependency to `tests` so that triggering the `tests` (`ninja/make
   tests`) target will build your test: `add_dependencies(tests my_test)`.
4. Use `add_test(NAME my_new_test COMMAND my_test <options>)` to declare a new
   test that will be run by `ctest` under name `my_new_test`.

In the simplest case, in an existing `CMakeLists.txt`, you might add the
following:

```bash
if(ENABLE_TESTS)
  add_executable(my_test mytest.c)
  add_dependencies(tests my_test)
  add_test(NAME my_new_test COMMAND my_test) # no options required
endif()
```

Note that this might get more complicated, e.g., typically you will have to
link some library into the executable with `target_link_libraries()`, or pass
some option to the test program.

`ctest` decides if a test passed via the return code of the program. So a test
executable that always passes is `int main() { return 0; }` and one that always
fails `int main() { return 1; }`. It is left as an exercise to the reader to
include these examples into `ctest`. Other programming languages other than C
or shell scripts are possible but discouraged. Obviously, though, a test in
a mainstream non-C programming language/shell script (C++, Rust, Python, Perl)
is preferable over no test.

Let's look at a more concrete, elaborate example, the NR RLC tests.
They are located in `openair2/LAYER2/nr_rlc/tests/`. Note that due to
historical reasons, a test script `run_tests.sh` allows to run all tests from
that directory directly, which you might also use to compare to the
`cmake`/`ctest` implementation.

1. Since the tests are in a sub-directory `tests/`, the inclusion of the entire
   directory is guarded in `openair2/LAYER2/nr_rlc/CMakeLists.txt` (in fact, it
   might in general be a good idea to create a separate sub-directory
   `tests/`!).
2. The NR RLC tests in fact consist of one "test driver program" (`test.c`)
   which is compiled with different "test stimuli" into the program. In total,
   there are 17 stimuli (`test1.h` to `test17.h`) with corresponding known
   "good" outputs after running (in `test1.txt.gz` to `test17.txt.gz`). To
   implement this, the `tests/CMakeLists.txt` creates multiple executables
   `nr_rlc_test_X` via the loop over `TESTNUM`, links necessary libraries into
   the test driver and a compile definition for the test stimuli.
3. For each executable, create a dependency to `tests`.
4. Finally, there is a single(!) `ctest` test that runs all the 17 test
   executables at once(!). If you look at the shell script
   `exec_nr_rlc_test.sh`, you see that it runs the program, filters for `TEST`,
   and compares with a predefined output from each test in `testX.txt.gz`,
   which is `gunzip`ed on the fly... Anyway, the actual `add_test()` definition
   just tells `ctest` to run this script (in the source directory), and passes
   an option where to find the executables (in the build directory). This
   slight complication is due to using shell scripts. An easier way is to
   directly declare the executable in `add_test()`, and `ctest` will locate and
   run the executable properly.

# Benchmarking

Google benchmark can be used to profile and benchmark small pieces of code. See
`benchmark_rotate_vector` for reference implementation. To start benchmarking code,
write a benchmark first and compare your implementation against baseline result.
To ensure your results are reproducible see this [guide](https://github.com/google/benchmark/blob/main/docs/reducing_variance.md)

Example output follows:

```bash
2024-08-26T11:55:49+02:00
Running ./openair1/PHY/TOOLS/tests/benchmark_rotate_vector
Run on (8 X 4700 MHz CPU s)
CPU Caches:
  L1 Data 48 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified 1280 KiB (x4)
  L3 Unified 12288 KiB (x1)
Load Average: 0.51, 0.31, 0.29
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
---------------------------------------------------------------------
Benchmark                           Time             CPU   Iterations
---------------------------------------------------------------------
BM_rotate_cpx_vector/100         43.1 ns         43.1 ns     16683136
BM_rotate_cpx_vector/256         70.1 ns         70.0 ns      9647446
BM_rotate_cpx_vector/1024         277 ns          277 ns      2378273
BM_rotate_cpx_vector/4096        1070 ns         1070 ns       654792
BM_rotate_cpx_vector/16384       4220 ns         4220 ns       169070
BM_rotate_cpx_vector/20000       5288 ns         5289 ns       136190
```

## Comparing results

Benchmark results can be output to json by using command line arguments, example below

```bash
./benchmark_rotate_vector --benchmark_out=file.json --benchmark_repetitions=10
```

These results can be compared by a tool provided with google benchmark

```bash
./compare.py benchmarks ../../file.json ../../file1.json
```

Example output:
```
Comparing ../../file.json to ../../file1.json
Benchmark                                           Time             CPU      Time Old      Time New       CPU Old       CPU New
--------------------------------------------------------------------------------------------------------------------------------
BM_rotate_cpx_vector/100                         +0.3383         +0.3384            43            58            43            58
BM_rotate_cpx_vector/100                         +0.2334         +0.2335            42            52            42            52
BM_rotate_cpx_vector/100                         +0.1685         +0.1683            42            49            42            49
BM_rotate_cpx_vector/100                         +0.1890         +0.1889            42            50            42            50
BM_rotate_cpx_vector/100                         +0.0456         +0.0457            42            44            42            44
BM_rotate_cpx_vector/100                         +0.0163         +0.0162            42            42            42            42
BM_rotate_cpx_vector/100                         +0.0005         +0.0004            43            43            43            43
BM_rotate_cpx_vector/100                         +0.0134         +0.0129            43            43            43            43
BM_rotate_cpx_vector/100                         +0.0162         +0.0162            42            42            42            42
BM_rotate_cpx_vector/100                         +0.0003         +0.0003            42            42            42            42
```
