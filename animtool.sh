#!/bin/sh

set -eu

usage() {
    echo "Usage: $0 [--ext EXT] [--open] FILE [OPTION ...]" >&2
    echo "The OPTIONs are those of bin/animtool, run it with --help to see them." >&2
}

ext=pdf
open=
while test "$#" -ge 1
do
    case "$1" in
        -h|--help) usage; exit 0;;
        --ext) ext="$2"; shift 2;;
        --open) open=1; shift;;
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



echo "Building animtool..." >&2
make bin/animtool

echo "Creating directory: $outdir" >&2
mkdir -p "$outdir"

echo "Creating dot file: $dotfile" >&2
bin/animtool "$@" "$infile" >"$dotfile" || exit

echo "Creating image: $outfile" >&2
dot -T"$ext" -o "$outfile" <"$dotfile"

if test -n "$open"
then
    echo "Opening image: $outfile" >&2
    xdg-open "$outfile"
fi
