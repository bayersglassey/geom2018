set -e
WIN_ID=`./get-x11-id.sh`
FPS=60
echo "Recording window $WIN_ID at $FPS fps..."
recordmydesktop --no-sound --no-cursor --fps "$FPS" --windowid "$WIN_ID"