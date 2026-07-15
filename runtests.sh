#!/bin/bash
# Super dumb test runner, used by Makefile.
set -euo pipefail
echo "Executing test suites: $@"
i=0
while test "$#" -ge 1
do
    TEST="$1"
    echo
    echo "###########################################################"
    echo "# EXECUTING TEST SUITE: $TEST"
    "$TEST"
    shift
    : $((i++))
done

echo
echo "###########################################################"
echo "# ALL $i TEST SUITES PASSED!"
