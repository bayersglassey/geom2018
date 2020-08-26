# Super dumb test runner, used by Makefile.
set -e
echo "Executing test suites: $@"
while test -n "$1"
do
    TEST="$1"
    echo
    echo "#################################"
    echo "# EXECUTING TEST SUITE: $TEST"
    "$TEST"
    shift
done