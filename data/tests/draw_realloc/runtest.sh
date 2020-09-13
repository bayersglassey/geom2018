for f in data/tests/draw_realloc/test*.fus
do
    echo
    echo "============================================"
    echo "== $f:"
    cat "$f"
    bin/collmaptool < "$f"
done
