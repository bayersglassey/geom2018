# WARNING: THIS IS UNUSED.
# It's a ganky thing which probably won't actually work.
# But it took too long to figure out how to achieve it (unix tools at
# their worst), so I'm gonna hold onto it in source control.
gcc -MM src/*.c | sed -E -e 's|^(.*.o:)|src/\1|'
