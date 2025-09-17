In this directory and subdirectories are various CI code test cases and helper
scripts.

# Unit test

There are some unit tests. From the parent directory, i.e., `ci-scripts/`,
start with

    python tests/deployment.py -v
    python tests/ping-iperf.py -v
    python tests/iperf-analysis.py -v

It will indicate if all tests passed. It assumes that these images are present:

- `oai-ci/oai-nr-ue:develop-12345678`
- `oai-ci/oai-gnb:develop-12345678`

# test-runner test

This is not a true test, because the results need to be manually inspected. To
run this "test", run

    ./run.sh

inside the `test-runner/` directory (important!).
