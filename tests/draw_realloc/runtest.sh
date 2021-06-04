collmaptool="$PWD/bin/collmaptool"
cd "$(dirname "$0")"

for f in test*.fus
do
    echo
    echo "============================================"
    echo "== $f:"
    cat "$f"
    "$collmaptool" < "$f"
done
