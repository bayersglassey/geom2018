#!/bin/bash
set -euo pipefail

do_with_log() {
    echo "=== Running: $@"
    "$@"
}

do_with_log bin/animtest test_data/animtest_data/*
