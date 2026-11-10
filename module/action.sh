#!/system/bin/sh
MODDIR=${0%/*}
/system/bin/sh "$MODDIR/control.sh" start || exit $?
/system/bin/sh "$MODDIR/bridge_control.sh" start || exit $?
/system/bin/sh "$MODDIR/control.sh" status
exec /system/bin/sh "$MODDIR/bridge_control.sh" probe
