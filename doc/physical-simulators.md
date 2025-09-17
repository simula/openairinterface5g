This document describes the physical simulators, also called "unitary
simulators", or short "physims".

[[_TOC_]]

# Introduction

There are several unitary simulations for the physical layer, testing
individual L1 transport/control channels using Monte-Carlo simulations, for
instance:

- `nr_pucchsim` for 5G PUCCH,
- `nr_dlsim` for 5G DLSCH/PDSCH,
- `nr_prachsim` for 5G PRACH,
- `dlsim` for 4G DLSCH/PDSCH, etc.

These simulators constitute the starting point for testing any new code in the
PHY layer, and are required tests to be performed before committing any new
code to the repository, in the sense that all of these should compile and
execute correctly. They provide estimates of the transport block error rates
and HARQ thoughput, DCI error rates, etc. The simulators reside in
`openair1/SIMULATION/LTE_PHY` and `openair1/SIMULATION/NR_PHY`. For instance,
`nr_dlsim` is built from `openair1/SIMULATION/NR_PHY/dlsim.c`.

Further unitary simulation of the coding subsystem components also exists in
`openair1/PHY/CODING/TESTBENCH`, e.g., `ldpctest` for encoding/decoding of
LDPC.

These are the simulators known to work properly and tested in CI:
- 4G: `dlsim`, `ulsim`
- 5G: `nr_dlsim`, `nr_ulsim`, `nr_dlschsim`, `nr_ulschsim`, `ldpctest`,
  `nr_pbchsim`, `nr_prachsim`, `nr_pucchsim`, `polartest`, `smallblocktest`,
  `nr_psbchsim` (sidelink)

# How to run

You first have to build simulators:

```bash
cd openairinterface5g/
. oaienv                 # important for the run_exec_autotests.bash script
cd cmake_targets/
./build_oai --ninja -P
```

This builds the simulators in the default build directory `ran_build/build`
(all paths given here are relative to `cmake_targets/` in the root of
`openairinterface5g/`). To run all simulators in this default build directory
of above `build_oai` invocation, run the `run_exec_autotests.bash` script:
```bash
autotests/run_exec_autotests.bash
```

This runs all simulations as defined in `autotests/test_case_list.xml`.

To run a group of tests, you can specify the group using `-g`. You can also
specify the build directory, after building the simulators, which, for the
default directory, is:

```bash
autotests/run_exec_autotests.bash -g "nr_dlsim.basic" -d ran_build/build/
```

This will run the `nr_dlsim.basic` group, as listed in
`autotests/test_case_list.xml`.

## How to interpret the output

From the output, you can see the invoked tests, its parameters, and the log files.
Example:

```
[user@oai cmake_targets]$ autotests/run_exec_autotests.bash -g "nr_dlsim.basic" -d ran_build/build/
[...]
Test case nr_dlsim.basic match found in group
name = nr_dlsim.basic
Description = nr_dlsim Test cases. (Test1: 106 PRB),
                                 (Test2: 217 PRB),
                                 (Test3: 273 PRB),
                                 (Test4: HARQ test 25% TP 4 rounds),
                                 (Test5: HARQ test 33% TP 3 rounds),
                                 (Test6: HARQ test 50% TP 2 rounds),
                                 (Test7: 25 PRBs, 15 kHz SCS)
main_exec = ran_build/build//nr_dlsim
main_exec_args = -n100 -R106 -b106 -s5
                      -n100 -R217 -b217 -s5
                      -n100 -R273 -b273 -s5
                      -n100 -s1 -S2 -t25
                      -n100 -s1 -S2 -t33
                      -n100 -s5 -S7 -t50
                      -n100 -m0 -e0 -R25 -b25 -i 2 1 0
search_expr_true = PDSCH test OK
search_expr_false = segmentation fault|assertion|exiting|fatal
tags = test1 test2 test3 test4 test5 test6 test7
nruns = 3
i = PDSCH test OK
Test1: 106 PRB
Executing test case nr_dlsim.basic.test1, Run Index = 1, Execution Log file = /home/oai/openairinterface5g/cmake_targets/autotests/log/nr_dlsim.basic/test.nr_dlsim.basic.test1.run_1
Executing test case nr_dlsim.basic.test1, Run Index = 2, Execution Log file = /home/oai/openairinterface5g/cmake_targets/autotests/log/nr_dlsim.basic/test.nr_dlsim.basic.test1.run_2
Executing test case nr_dlsim.basic.test1, Run Index = 3, Execution Log file = /home/oai/openairinterface5g/cmake_targets/autotests/log/nr_dlsim.basic/test.nr_dlsim.basic.test1.run_3
[...]
nr_dlsim.basic.test1 RUN = 1 Result = PASS
nr_dlsim.basic.test1 RUN = 2 Result = PASS
nr_dlsim.basic.test1 RUN = 3 Result = PASS
execution nr_dlsim.basic.test1 {Test1: 106 PRB} Run_Result = " Run_1 =PASS Run_2 =PASS Run_3 =PASS"  Result = PASS
```

You see:
- the `Description` of the test cases (here, 7 tests)
- the executable that is used, and the parameters (`main_exec` and
  `main_exec_args`), for instance `-n100 -R106 -b106 -s5` for `Test1`
- what is the condition for a successful test
- where are the log files
- if the tests passed or not `PASS`, otherwise `FAIL` and a summary for the
  three runs.

## How to change the parameters

You can modify the parameters in `autotests/test_case_list.xml`. To learn about
their meaning, run the simulator manually with flag `-h` ("help"). For
instance, for the above run, it would be

```bash
ran_build/build/nr_dlsim -h
```

## How to change the code

You can modify the source files to adapt to your needs. For instance, for
`nr_dlsim` above, refer to file `openair1/SIMULATION/NR_PHY/dlsim.c`.

# CI

These tests are run in this pipeline:
[RAN-PhySim-Cluster](https://jenkins-oai.eurecom.fr/job/RAN-PhySim-Cluster/).
