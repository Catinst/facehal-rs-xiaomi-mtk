#!/system/bin/sh
SKIPUNZIP=0
ui_print "- K50 ColorOS17 FaceHAL registration test"
ui_print "- Private libraries only; no system/vendor/odm overlay"
ui_print "- Default: system-test; use module Action after install"
ui_print "- This installer does not reboot or clear face data"
[ -x /data/adb/ksud ] || abort "KernelSU is required"
SDK=$(getprop ro.build.version.sdk)
case "$SDK" in ''|*[!0-9]*) abort "Invalid Android SDK property" ;; esac
if [ "$SDK" -lt 37 ]; then
    abort "Android API 37 ColorOS17 is required for this tested binary"
fi
if [ "$(getprop ro.product.cpu.abi)" != "arm64-v8a" ]; then
    abort "arm64-v8a is required"
fi
set_perm_recursive "$MODPATH" 0 0 0755 0644
for script in control.sh bridge_control.sh service.sh action.sh uninstall.sh; do
    set_perm "$MODPATH/$script" 0 0 0755
 done
set_perm "$MODPATH/bin/facehal" 0 0 0755
set_perm "$MODPATH/bin/binder_client" 0 0 0755
set_perm "$MODPATH/bin/virtual_bridge" 0 0 0755
set_perm "$MODPATH/bin/bridge_client" 0 0 0755
mkdir -p "$MODPATH/run" "$MODPATH/logs"
set_perm "$MODPATH/run" 0 0 0700
set_perm "$MODPATH/logs" 0 0 0700
