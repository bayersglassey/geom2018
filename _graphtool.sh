#!/bin/sh
#
# Expects the env var TOOL to be set, e.g. to "animtool"
#
set -euo pipefail

usage() {
    echo "Usage: $0 [-e|--ext EXT] [-o|--open] FILE [OPTION ...]" >&2
    echo "The OPTIONs are those of bin/$TOOL, run it with --help to see them." >&2
}

ext=pdf
open=
while test "$#" -ge 1
do
    case "$1" in
        -h|--help) usage; exit 0;;
        -e|--ext) ext="$2"; shift 2;;
        -o|--open) open=1; shift;;
        --) shift; break;;
        *) break;;
    esac
done

test "$#" -ge 1 || {
    usage
    exit 1
}

infile="$1"
shift

filename="$(basename "$infile")"
outdir="output"
dotfile="$outdir/$filename.dot"
outfile="$outdir/$filename.$ext"



echo "Building $TOOL..." >&2
make "bin/$TOOL"

echo "Creating directory: $outdir" >&2
mkdir -p "$outdir"

echo "Creating dot file: $dotfile" >&2
"bin/$TOOL" "$@" "$infile" >"$dotfile" || exit

echo "Creating image: $outfile" >&2
dot -T"$ext" -o "$outfile" <"$dotfile"

if test -n "$open"
then
    echo "Opening image: $outfile" >&2
    xdg-open "$outfile"
fi
