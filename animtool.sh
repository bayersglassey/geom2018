#!/bin/sh

set -eu

usage() {
    echo "Usage: $0 [--ext EXT] [--open] ANIM [OPTION ...]" >&2
    echo "...where the file anim/ANIM.fus is expected to exist." >&2
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

anim="$1"
shift

outdir="output"
infile="anim/$anim.fus"
dotfile="$outdir/$anim.dot"
outfile="$outdir/$anim.$ext"



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
