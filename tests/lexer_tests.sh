#!/bin/sh
set -euo pipefail

TOOL="bin/lexertool"

NL='
'

TEST=0

msg() {
    echo "$1" >&2
}

die() {
    msg "$1"
    exit 1
}

assert_eq() {
    msg "Test $((TEST=TEST+1))"
    test "($1)" = "($2)" || die "Assertion failed! These are not equal:$NL---THIS:$NL$1$NL---AND THIS:$NL$2"
}

assert_eq "$(echo '1 2 3' | "$TOOL")" "1${NL}2${NL}3"
assert_eq "$(echo '1 2 3' | "$TOOL" -r)" "1 2 3"

assert_eq "$(echo '1 (2 3) 4' | "$TOOL")" "1${NL}:${NL}    2${NL}    3${NL}4"
assert_eq "$(echo 'x(y(1) z(2))' | "$TOOL")" "x${NL}:${NL}    y${NL}    :${NL}        1${NL}    z${NL}    :${NL}        2"

assert_eq "$(echo '$SET_INT X 2 $GET_INT X' | "$TOOL" -r)" '$SET_INT X 2 $GET_INT X'
assert_eq "$(echo '$SET_INT X 2 $GET_INT X' | "$TOOL" -v -r)" '2'

msg "Test suite ok!"
