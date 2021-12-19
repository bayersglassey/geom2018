set -e

dst="out.mp4"

delay=2
echo "Waiting $delay seconds before starting the recording..." >&2
sleep "$delay"

# "https://wiki.videolan.org/Codec/" sayeth:
# "For portability to almost all decoders, we recommend using the MPEG-1 standard of vcodec=mp1v, acodec=mpga, and mux=mpeg1"
# Should we perhaps change our vcodec from h264 to mp1v?..
cvlc screen:// \
    --screen-fps=30 \
    --sout-x264-preset fast \
    --sout-x264-tune animation \
    --sout "#transcode{vcodec=h264}:file{dst=$dst}"
