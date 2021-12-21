#!/bin/bash

set -euo pipefail

test "$#" -ge 2 || {
    echo "Usage: rename_thing.sh RENAME_FROM RENAME_TO" >&2
    exit 1
}

from="$1"
to="$2"
shift 2


echo "### Renaming: $from -> $to" >&2


if ack -l "$from" "$@" >/dev/null
then
    echo "Modifying the following files:" >&2
    ack -l "$from" "$@" >&2
    sed -i "s@$from@$to@g" $(ack -l "$from" "$@") || true
else
    echo "No files need to be modified" >&2
fi

found=
for f in $(find "$@" -name "*$from*")
do
    found=1
    f2="$(echo "$f" | sed "s@$from@$to@g")"
    echo "Moving: $f -> $f2"
    mv "$f" "$f2"
done

if test -z "$found"
then
    echo "No files need to be moved" >&2
fi

echo "OK" >&2
