#!/system/bin/sh
MODDIR=${0%/*}
/system/bin/sh "$MODDIR/bridge_control.sh" stop
/system/bin/sh "$MODDIR/control.sh" stop
exit 0
