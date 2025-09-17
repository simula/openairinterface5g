#!/bin/bash

set -eo pipefail

test -f /opt/oai-gnb/nrL1_stats.log && grep -oP 'PRACH I0 = \K[0-9.]*' /opt/oai-gnb/nrL1_stats.log | awk '{if ($1 > 0.0) exit 0; else exit 1}'
