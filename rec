set -e

OUTFILE="out.gif"

WININFO=`xwininfo -name "Depths of Uo"`
echo "$WININFO"
WIX=`echo "$WININFO" | sed -nr -e 's/^.*Absolute upper-left X: +([0-9]+).*$/\1/p'`
WIY=`echo "$WININFO" | sed -nr -e 's/^.*Absolute upper-left Y: +([0-9]+).*$/\1/p'`
WIW=`echo "$WININFO" | sed -nr -e 's/^.*Width: +([0-9]+).*$/\1/p'`
WIH=`echo "$WININFO" | sed -nr -e 's/^.*Height: +([0-9]+).*$/\1/p'`
echo "wininfo: x, y, w, h = $WIX, $WIY, $WIW, $WIH"

w=512
h=384
x=$(expr $WIX + '(' $WIW - $w ')' / 2)
y=$(expr $WIY + '(' $WIH - $h ')' / 2)

rm "$OUTFILE" 2>/dev/null && true

echo "Will record to: $OUTFILE"
byzanz-record -v -x "$x" -y "$y" -w "$w" -h "$h" --delay 3 "$OUTFILE"
