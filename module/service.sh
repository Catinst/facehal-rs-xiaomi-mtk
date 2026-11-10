#!/system/bin/sh
MODDIR=${0%/*}
count=0
while [ "$(getprop sys.boot_completed)" != "1" ]; do
    [ -e "$MODDIR/disable" ] && exit 0
    [ -e "$MODDIR/remove" ] && exit 0
    count=$((count + 1))
    [ "$count" -ge 120 ] && exit 1
    sleep 1
done
[ -e "$MODDIR/disable" ] && exit 0
[ -e "$MODDIR/remove" ] && exit 0
/system/bin/sh "$MODDIR/control.sh" start || exit $?
exec /system/bin/sh "$MODDIR/bridge_control.sh" start
