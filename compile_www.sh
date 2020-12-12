set -e

RELEASE=0
if [ "$1" = "release" ]
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


# We've already got a modified version of emcc's main.html in www/,
# so get rid of the unmodified version it generates:
EMCC_FILES=$(echo main.{html,html.map})
echo "Removing files: $EMCC_FILES"
rm -f $EMCC_FILES

# Move the following files into www/:
WWW_EMCC_FILES=$(echo main.{js,html.mem,data})
echo "Moving files into www: $WWW_EMCC_FILES"
for f in $WWW_EMCC_FILES
do
    test -f "$f" || continue
    mv $f www/
done

