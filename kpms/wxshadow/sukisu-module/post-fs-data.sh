#!/system/bin/sh
set -e
MODDIR=${0%/*}
chmod 755 "$MODDIR/wxshadow_client"
"$MODDIR/wxshadow_client" --enable
