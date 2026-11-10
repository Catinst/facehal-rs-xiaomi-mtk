#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <android/log.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef AIBinder *(*check_fn)(const char *);
typedef int32_t (*add_fn)(AIBinder *, const char *);
typedef bool (*pool_fn)(uint32_t);
typedef void (*join_fn)(void);

struct Bridge { AIBinder *face; check_fn check; pthread_mutex_t mutex; };
static void *create(void *ctx) { return ctx; }
static void destroy(void *ctx) { (void)ctx; }

static binder_status_t transact(AIBinder *binder, transaction_code_t code,
                               const AParcel *in, AParcel *out) {
    (void)in;
    struct Bridge *bridge = AIBinder_getUserData(binder);
    fprintf(stderr, "FaceVirtualBridge transaction=%u\n", code);
    __android_log_print(ANDROID_LOG_INFO, "FaceVirtualBridge", "transaction=%u", code);
    if (code != 23) {
        fprintf(stderr, "FaceVirtualBridge: unsupported control transaction=%u\n", code);
        return STATUS_UNKNOWN_TRANSACTION;
    }
    if (!bridge) return STATUS_DEAD_OBJECT;
    pthread_mutex_lock(&bridge->mutex);
    if (bridge->face) AIBinder_decStrong(bridge->face);
    bridge->face = bridge->check("android.hardware.biometrics.face.IFace/default");
    AIBinder *face = bridge->face;
    if (face) AIBinder_incStrong(face);
    pthread_mutex_unlock(&bridge->mutex);
    if (!face) return STATUS_DEAD_OBJECT;
    AStatus *status = AStatus_newOk();
    binder_status_t result = AParcel_writeStatusHeader(out, status);
    AStatus_delete(status);
    if (result == STATUS_OK) result = AParcel_writeStrongBinder(out, face);
    AIBinder_decStrong(face);
    fprintf(stderr, "FaceVirtualBridge getFaceHal -> real IFace/default status=%d\n", result);
    return result;
}

int main(void) {
    const char *target = "android.hardware.biometrics.face.IFace/default";
    const char *name = "android.hardware.biometrics.face.virtualhal.IVirtualHal/virtual";
    void *lib = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "binder dlopen: %s\n", dlerror()); return 2; }
    check_fn check = (check_fn)dlsym(lib, "AServiceManager_checkService");
    add_fn add = (add_fn)dlsym(lib, "AServiceManager_addService");
    pool_fn pool = (pool_fn)dlsym(lib, "ABinderProcess_setThreadPoolMaxThreadCount");
    join_fn join = (join_fn)dlsym(lib, "ABinderProcess_joinThreadPool");
    if (!check || !add || !pool || !join) return 3;
    AIBinder *existing = check(name);
    if (existing) {
        AIBinder_decStrong(existing);
        fprintf(stderr, "FaceVirtualBridge: existing virtual service left untouched\n");
        return 4;
    }
    struct Bridge bridge = { .face = check(target), .check = check, .mutex = PTHREAD_MUTEX_INITIALIZER };
    if (!bridge.face) { fprintf(stderr, "FaceVirtualBridge: real IFace/default absent\n"); return 5; }
    AIBinder_Class *cls = AIBinder_Class_define(
        "android.hardware.biometrics.face.virtualhal.IVirtualHal", create, destroy, transact);
    AIBinder *binder = cls ? AIBinder_new(cls, &bridge) : NULL;
    if (!binder) { AIBinder_decStrong(bridge.face); return 6; }
    pool(4);
    // IVirtualHal is an unstable framework test interface; no fake stable version/hash.
    int32_t exception = add(binder, name);
    fprintf(stderr, "FaceVirtualBridge addService raw=%d pid=%d exception=%s\n", exception, getpid(),
            exception == 0 ? "EX_NONE" : "EX_ERROR");
    if (exception != 0) {
        AIBinder_decStrong(binder);
        AIBinder_decStrong(bridge.face);
        return 7;
    }
    join();
    AIBinder_decStrong(binder);
    AIBinder_decStrong(bridge.face);
    return 0;
}
