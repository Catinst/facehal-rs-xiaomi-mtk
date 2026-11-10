#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef AIBinder *(*check_fn)(const char *);
static void *create(void *arg) { return arg; }
static void destroy(void *arg) { (void)arg; }
static binder_status_t unused(AIBinder *b, transaction_code_t c,
                             const AParcel *in, AParcel *out) {
    (void)b; (void)c; (void)in; (void)out;
    return STATUS_UNKNOWN_TRANSACTION;
}
static bool string_alloc(void *ctx, int32_t size, char **out) {
    if (size < 0 || size > 4096) return false;
    char *buffer = calloc((size_t)size + 1, 1);
    if (!buffer) return false;
    *(char **)ctx = buffer;
    *out = buffer;
    return true;
}

static AParcel *call(AIBinder *binder, transaction_code_t code) {
    AParcel *in = NULL, *out = NULL;
    binder_status_t status = AIBinder_prepareTransaction(binder, &in);
    if (status != STATUS_OK) { printf("prepare code=%u status=%d\n", code, status); return NULL; }
    status = AIBinder_transact(binder, code, &in, &out, 0);
    printf("transact code=%u status=%d\n", code, status);
    if (status != STATUS_OK) { if (out) AParcel_delete(out); return NULL; }
    AStatus *header = NULL;
    status = AParcel_readStatusHeader(out, &header);
    if (status != STATUS_OK || !header || !AStatus_isOk(header)) {
        printf("reply code=%u parcelStatus=%d exception=%d\n", code, status,
               header ? AStatus_getExceptionCode(header) : -999);
        if (header) AStatus_delete(header);
        AParcel_delete(out);
        return NULL;
    }
    AStatus_delete(header);
    return out;
}

int main(int argc, char **argv) {
    void *lib = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_LOCAL);
    check_fn check = lib ? (check_fn)dlsym(lib, "AServiceManager_checkService") : NULL;
    if (!check) return 2;
    AIBinder *binder = NULL;
    if (argc > 1 && strcmp(argv[1], "--virtual") == 0) {
        AIBinder *bridge = check("android.hardware.biometrics.face.virtualhal.IVirtualHal/virtual");
        if (!bridge) { puts("VirtualHal absent"); return 3; }
        AIBinder_Class *virtual_cls = AIBinder_Class_define(
            "android.hardware.biometrics.face.virtualhal.IVirtualHal", create, destroy, unused);
        if (!virtual_cls || !AIBinder_associateClass(bridge, virtual_cls)) return 4;
        AParcel *reply = call(bridge, 23);
        if (!reply || AParcel_readStrongBinder(reply, &binder) != STATUS_OK || !binder) return 10;
        AParcel_delete(reply);
        AIBinder_decStrong(bridge);
        puts("VirtualHal getFaceHal returned a real binder");
    } else {
        binder = check("android.hardware.biometrics.face.IFace/default");
    }
    if (!binder) { puts("IFace service absent"); return 3; }
    AIBinder_Class *cls = AIBinder_Class_define("android.hardware.biometrics.face.IFace",
                                              create, destroy, unused);
    if (!cls || !AIBinder_associateClass(binder, cls)) return 4;
    AParcel *version = call(binder, 16777215);
    int32_t value = -1;
    if (!version || AParcel_readInt32(version, &value) != STATUS_OK) return 5;
    printf("interfaceVersion=%d\n", value);
    AParcel_delete(version);
    if (value != 4) return 6;
    AParcel *hash = call(binder, 16777214);
    char *text = NULL;
    if (!hash || AParcel_readString(hash, &text, string_alloc) != STATUS_OK || !text) return 7;
    printf("interfaceHash=%s\n", text);
    int hash_ok = strcmp(text, "c43fbb9be4a662cc9ace640dba21cccdb84c6c21") == 0;
    free(text);
    AParcel_delete(hash);
    if (!hash_ok) return 8;
    AParcel *props = call(binder, 1);
    int32_t count = -1;
    if (!props || AParcel_readInt32(props, &count) != STATUS_OK || count != 1) return 9;
    printf("sensorPropsCount=%d\n", count);
    for (int i = 0; i < 14; ++i) {
        int32_t word;
        if (AParcel_readInt32(props, &word) != STATUS_OK) break;
        printf("sensorPropsRaw[%d]=%d hex=0x%08x\n", i, word, (unsigned)word);
    }
    AParcel_delete(props);
    AIBinder_decStrong(binder);
    puts("IFace version/hash/props transport PASS");
    return 0;
}
