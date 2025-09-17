# Colosseum Automated Testing

These scripts are used by a Jenkins [job](../Jenkinsfile-colosseum) to trigger automated testing of OpenAirInterface (OAI) gNB and softUE on the [Colosseum](https://www.northeastern.edu/colosseum/) Open RAN digital twin.
Once a test is triggered, a new OAI LXC container at the specified OAI version will be built on Colosseum (if not already present), and gNB and softUE will perform TCP uplink and downlink connectivity test via the iPerf [tool](https://iperf.fr/).
The OAI branch to build and test can be specified through the `eNB_Branch` parameter passed through Jenkins (`eNB_TargetBranch`, which defaults to the `develop` branch, is used if `eNB_Branch` is not specified).
The Colosseum network scenario to test is specified through the `Colosseum_Rf_Scenario` Jenkins parameter, which defaults to a base Colosseum scenario without artificially added channel effects (e.g., only hardware impariments of software-defined radios, cables, and channel emulator).

Once the test ends, results are analyzed through the OAI automated test report generation tool available [here](https://github.com/ztouchnetworks/openairinterface-automated-test-reports), which builds a test report from the iPerf and OAI logs, and marks the test as successful or unsuccessful.
Results from successful tests are saved in history files and used to compare more recent tests.
A test is considered successful if the downlink throughput achieved during the test is greather than or equal to the average of the test history, which spans successful test executed since May 2024.
This is used to identify possible regressions of OAI runs executed on the Colosseum testbed.
