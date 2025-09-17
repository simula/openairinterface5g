#!/bin/bash

# This bash script builds an awk script. The awk script tries to match
# (provided) patterns and compares them against given thresholds condition
# (e.g., threshold is "< 10").

function die() { echo $@ 1>&2; exit 1; }

# RC will be the return code. If any rule fails, it will set RC=1, which will
# make the script fail. Print also every line, because the logs are piped into
# this script, but a user is typically also interested into the raw logs.
SCRIPT='
  BEGIN { RC = 0; }

  { print $0 }
'

NUM=0
# for each pair of <pattern>/<condition>, add corresponding rules.
while [ $# -gt 0 ]; do
  [ $# -ne 1 ] || die "unmatched <pattern>/<condition>"
  PATTERN=${1}
  COND=${2}
  shift 2

  # Add a rule that searches for a PATTERN + number, and checks against
  # CONDition. If the condition does not hold, it is counted as a failure (sets
  # RC to signal error). In both cases, the result is logged in an array to
  # output at the end of the script.
  #
  # Example: pattern "PHY proc tx", condition "< 200"
  # The awk script tries to match every line for "PHY proc tx _NUMBER_" (where
  # number is a decimal), and compares _NUMBER_ against condition "< 200",
  # i.e., "_NUMBER_ < 200".
  # If the condition holds, will set "CHECK PHY proc tx _NUMBER_ < 200 SUCCESS".
  # If the condition fails, will set "CHECK PHY proc tx _NUMBER_ < 200 FAIL".
  SCRIPT+='
  match($0, /'${PATTERN}' +([0-9]+(\.[0-9]+)?)/, n) {
    if (n[1] '${COND}') {
      r = "SUCCESS";
    } else {
      r = "FAIL";
      RC = 1;
    }
    RESULTS['${NUM}']=sprintf("CHECK %-35s %7.2f %-8s %s", "'${PATTERN}'", n[1], " '${COND}'", r);
  }
'

  # Generate an additional rule for the end of the logs: if the pattern is not
  # found, it is counted as a failure and logged appropriately.
  # Following the above example "PHY proc tx" and "< 200": if such log is not
  # found, will set "CHECK PHY proc tx < 200 NOTFOUND"
  SCRIPT+='
  END {
    if (!RESULTS['${NUM}']) {
      RESULTS['${NUM}']=sprintf("CHECK %-35s          %-7s NOTFOUND", "'${PATTERN}'", "'${COND}'");
      RC = 1;
    }
  }
'

  let NUM=${NUM}+1
done

# After passing through all logs, print all results, and exit with return code
# (0 on success, i.e..all conditions checked, otherwise 1 on failure).
SCRIPT+='
  END {
    for (i = 0; i < '${NUM}'; ++i) {
      print RESULTS[i]
    }
    exit RC
  }
'

# Read from separate file descriptor 3: this allows us to pipe the actual log
# to analyze into awk (by using the script like so: analyze-timing.sh < log)
awk -f /dev/fd/3 3<<< ${SCRIPT}
