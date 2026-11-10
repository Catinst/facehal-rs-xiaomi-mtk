#!/system/bin/sh
set -u
MODDIR=$(readlink -f "${0%/*}") || exit 1
BIN="$MODDIR/bin/virtual_bridge"
RUNDIR="$MODDIR/run/virtual_bridge"
LOGDIR="$MODDIR/logs/virtual_bridge"
STATE="$RUNDIR/identity"
BB=/data/adb/ksu/bin/busybox
mkdir -p "$RUNDIR" "$LOGDIR" || exit 1
chmod 0700 "$RUNDIR" "$LOGDIR"

load_identity() {
    PID= TICK=
    [ -r "$STATE" ] || return 1
    read -r PID TICK < "$STATE" || return 1
    case "$PID:$TICK" in *[!0-9:]*|:*|*:) return 1 ;; esac
    [ "$PID" -gt 1 ] || return 1
    [ "$(readlink "/proc/$PID/exe" 2>/dev/null)" = "$BIN" ] || return 1
    [ "$(awk '{print $22}' "/proc/$PID/stat" 2>/dev/null)" = "$TICK" ] || return 1
    kill -0 "$PID" 2>/dev/null
}

owned_spawn() {
    [ -n "$HALTICK" ] || return 1
    [ "$(awk '{print $22}' "/proc/$HALPID/stat" 2>/dev/null)" = "$HALTICK" ] || return 1
    [ "$(awk '{print $3}' "/proc/$HALPID/stat" 2>/dev/null)" != Z ] || return 1
    kill -0 "$HALPID" 2>/dev/null
}

owned_identity() {
    [ "$(readlink "/proc/$HALPID/exe" 2>/dev/null)" = "$BIN" ] || return 1
    owned_spawn
}

with_lock() {
    export FACEHAL_CONTROL_LOCKED=1
    exec "$BB" flock -n "$RUNDIR/control.lock" /system/bin/sh "$MODDIR/bridge_control.sh" "$1"
}

acquire_lock() {
    [ "${FACEHAL_CONTROL_LOCKED:-}" = 1 ] || return 2
    trap 'exit 143' HUP INT TERM
}

status() {
    if load_identity; then
        printf 'virtual_bridge=running pid=%s starttime=%s\n' "$PID" "$TICK"
    else
        printf 'virtual_bridge=stopped\n'
    fi
    printf 'mode=%s boot_completed=%s selinux=%s\n' \
        "$(head -n 1 "$MODDIR/mode.conf")" "$(getprop sys.boot_completed)" "$(getenforce)"
    tail -n 12 "$LOGDIR/facehal.log" 2>/dev/null || true
}

probe() {
    load_identity || { status; return 1; }
    grep -q 'raw=0 .*exception=EX_NONE' "$LOGDIR/facehal.log" || return 1
    (unset LD_LIBRARY_PATH LD_PRELOAD; timeout 10 "$MODDIR/bin/bridge_client" --virtual) \
        > "$LOGDIR/probe.log" 2>&1
    result=$?
    cat "$LOGDIR/probe.log"
    [ "$result" -eq 0 ] && load_identity
}

start() {
    [ -e "$MODDIR/disable" ] && { echo 'module is disabled'; return 1; }
    [ -e "$MODDIR/remove" ] && { echo 'module is marked for removal'; return 1; }
    acquire_lock || return $?
    if load_identity; then status; return 0; fi
    if ! timeout 5 service list > "$LOGDIR/services.before" 2>&1; then
        echo 'service enumeration failed; leaving existing services untouched'
        return 1
    fi
    if grep -Fq 'android.hardware.biometrics.face.virtualhal.IVirtualHal/virtual' "$LOGDIR/services.before"; then
        echo 'an existing IFace/default service is present; leaving it untouched'
        return 1
    fi
    rm -f "$STATE"
    : > "$LOGDIR/facehal.log"
    : > "$LOGDIR/supervisor.log"
    TOKEN="$$.$(awk '{print $22}' /proc/$$/stat)"
    printf '%s\n' "$TOKEN" > "$RUNDIR/start.token"
    # The file-path flock test confirmed FD3; close non-stdio descriptors in the daemon.
    setsid /system/bin/sh "$MODDIR/bridge_control.sh" __run "$TOKEN" \
        3>&- 4>&- 5>&- 6>&- 7>&- 8>&- 9>&- </dev/null >> "$LOGDIR/supervisor.log" 2>&1 &
    count=0
    while [ "$count" -lt 10 ]; do
        if load_identity && grep -q 'raw=0 .*exception=EX_NONE' "$LOGDIR/facehal.log"; then
            status
            return 0
        fi
        count=$((count + 1))
        sleep 1
    done
    echo 'facehal startup did not complete; inspect logs'
    if load_identity; then
        HALPID=$PID HALTICK=$TICK
        owned_identity && kill "$HALPID" 2>/dev/null || true
    fi
    tail -n 30 "$LOGDIR/facehal.log" "$LOGDIR/supervisor.log" 2>/dev/null || true
    return 1
}

stop() {
    acquire_lock || return $?
    if ! load_identity; then echo 'facehal already stopped'; return 0; fi
    HALPID=$PID HALTICK=$TICK
    owned_identity && kill "$HALPID" 2>/dev/null || true
    count=0
    while owned_identity && [ "$count" -lt 5 ]; do
        count=$((count + 1)); sleep 1
    done
    if owned_identity; then
        owned_identity && kill -KILL "$HALPID" 2>/dev/null || true
    fi
    count=0
    while owned_identity && [ "$count" -lt 20 ]; do
        count=$((count + 1)); sleep 0.1
    done
    if owned_identity; then echo 'module-owned facehal is still running'; return 1; fi
    echo 'module-owned facehal stopped'
}

run() {
    [ -n "${2:-}" ] && [ -r "$RUNDIR/start.token" ] || exit 2
    [ "$(head -n 1 "$RUNDIR/start.token")" = "$2" ] || exit 2
    rm -f "$RUNDIR/start.token"
    [ -e "$MODDIR/disable" ] && exit 1
    [ -e "$MODDIR/remove" ] && exit 1
    load_identity && exit 1
    MODE=$(head -n 1 "$MODDIR/mode.conf")
    case "$MODE" in system-test|vintf) ;; *) echo 'invalid mode.conf'; exit 1 ;; esac
    export LD_LIBRARY_PATH="$MODDIR/lib64"
    export FACEHAL_ALGORITHM_LIBRARY="$MODDIR/lib64/libjni_stfaceunlock_api.so"
    if [ "$MODE" = system-test ]; then export FACEHAL_BINDER_MODE=system-test
    else unset FACEHAL_BINDER_MODE; fi
    "$BIN" </dev/null >> "$LOGDIR/facehal.log" 2>&1 &
    HALPID=$!
    HALTICK=$(awk '{print $22}' "/proc/$HALPID/stat" 2>/dev/null)
    trap 'owned_spawn && kill "$HALPID" 2>/dev/null || true' HUP INT TERM
    count=0
    while owned_spawn && ! owned_identity && [ "$count" -lt 50 ]; do
        count=$((count + 1)); sleep 0.1
    done
    if ! owned_identity; then
        owned_spawn && kill "$HALPID" 2>/dev/null || true
        count=0
        while owned_spawn && [ "$count" -lt 20 ]; do
            count=$((count + 1)); sleep 0.1
        done
        owned_spawn && kill -KILL "$HALPID" 2>/dev/null || true
        echo 'HAL exec identity was not established'
        exit 1
    fi
    printf '%s %s\n' "$HALPID" "$HALTICK" > "$STATE.tmp"
    mv "$STATE.tmp" "$STATE"
    wait "$HALPID"
    result=$?
    if owned_spawn; then
        owned_spawn && kill "$HALPID" 2>/dev/null || true
        count=0
        while owned_spawn && [ "$count" -lt 20 ]; do
            count=$((count + 1)); sleep 0.1
        done
        owned_spawn && kill -KILL "$HALPID" 2>/dev/null || true
        count=0
        while owned_spawn && [ "$count" -lt 20 ]; do
            count=$((count + 1)); sleep 0.1
        done
        if owned_spawn; then
            echo 'HAL remains alive; retaining its identity for a later stop'
            exit 1
        fi
    fi
    if [ -r "$STATE" ] && [ "$(head -n 1 "$STATE")" = "$HALPID $HALTICK" ]; then
        rm -f "$STATE"
    fi
    printf 'facehal_exit=%s\n' "$result"
    exit "$result"
}

case "${1:-status}" in
    start) unset FACEHAL_CONTROL_LOCKED; with_lock __start ;;
    stop) unset FACEHAL_CONTROL_LOCKED; with_lock __stop ;;
    __start) start ;;
    __stop) stop ;;
    status) status ;;
    probe) probe ;;
    __run) run "$@" ;;
    *) echo 'usage: control.sh start|stop|status|probe'; exit 2 ;;
esac
