set -euo pipefail

RELEASE=0
if [ "${1:-}" = "release" ]
then
    RELEASE=1
    shift
    echo "Starting a release build..."
else
    echo "Starting a debug build..."
fi

MAIN_C="src/main/demo.c"

CFLAGS=""
CFLAGS+=" -std=c99 -Wall -Werror -Wno-unused -Wno-missing-braces -Wno-tautological-compare -Wno-parentheses"

CC="emcc"
OUTFILE="main.html"
CFLAGS+=" -DNO_EXECINFO"
CFLAGS+=" -s USE_SDL=2 "
CFLAGS+=" --preload-file data --preload-file actor --preload-file anim"

#CFLAGS+=" -s TOTAL_MEMORY=1073741824" # 1024*1024*1024 = 1G
CFLAGS+=" -s ALLOW_MEMORY_GROWTH=1"

if [ "$RELEASE" = 1 ]
then
    CFLAGS+=" -O2 -g"
else
    CFLAGS+=" -O0 -g"
fi

"$CC" $CFLAGS src/*.c "$MAIN_C" -o "$OUTFILE" $@


##########################################################################
# Building www directory

WWW_SRC_DIR="www-src"
WWW_DIR="www"

# We've already got a modified version of emcc's main.html in $WWW_SRC_DIR,
# so get rid of the unmodified version generated by emcc just now:
EMCC_FILES=$(echo main.{html,html.map})
echo "Removing files: $EMCC_FILES"
rm -f $EMCC_FILES

# Remove old $WWW_DIR, make fresh one
echo "Resetting $WWW_DIR/"
rm -rf "$WWW_DIR"
cp -r "$WWW_SRC_DIR" "$WWW_DIR"

# Move files generated by emcc into $WWW_DIR
for f in main.{js,html.mem,data,wasm}
do
    echo "Moving $f into $WWW_DIR/"
    test -f "$f" || continue
    mv "$f" "$WWW_DIR"/
done

