#include "../../../../native/FacehalLog.h"
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct AIBinder AIBinder;
typedef struct AIBinder_Class AIBinder_Class;
typedef struct AParcel AParcel;
typedef int32_t binder_status_t;

typedef bool (*set_thread_count_fn)(uint32_t);
typedef void (*void_fn)(void);
typedef int32_t (*add_service_fn)(AIBinder*, const char*);
typedef bool (*is_declared_fn)(const char*);
typedef void (*set_requesting_sid_fn)(AIBinder*, bool);
typedef void (*mark_vintf_stability_fn)(AIBinder*);
typedef const char* (*get_class_descriptor_fn)(const AIBinder_Class*);
typedef AParcel* (*parcel_create_fn)(void);
typedef int32_t (*parcel_get_data_size_fn)(const AParcel*);
typedef binder_status_t (*parcel_append_from_fn)(const AParcel*, AParcel*, int32_t, int32_t);

static void* binder_ndk_handle(void) {
    static void* handle;
    if (handle == NULL) {
        handle = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_LOCAL);
    }
    return handle;
}

static void* binder_ndk_symbol(const char* name) {
    void* symbol = dlsym(RTLD_NEXT, name);
    if (symbol == NULL) {
        void* handle = binder_ndk_handle();
        if (handle != NULL) {
            symbol = dlsym(handle, name);
        }
    }
    return symbol;
}

static void missing_symbol(const char* name) {
    fprintf(stderr, "facehal: missing baseline libbinder_ndk symbol: %s\n", name);
    abort();
}

bool facehal_binder_ndk_has_api31(void) {
    return binder_ndk_symbol("AParcel_create") != NULL &&
            binder_ndk_symbol("AParcel_getDataSize") != NULL &&
            binder_ndk_symbol("AParcel_appendFrom") != NULL;
}

bool ABinderProcess_setThreadPoolMaxThreadCount(uint32_t num_threads) {
    set_thread_count_fn function =
            (set_thread_count_fn)binder_ndk_symbol("ABinderProcess_setThreadPoolMaxThreadCount");
    if (function == NULL) missing_symbol("ABinderProcess_setThreadPoolMaxThreadCount");
    return function(num_threads);
}

void ABinderProcess_startThreadPool(void) {
    void_fn function = (void_fn)binder_ndk_symbol("ABinderProcess_startThreadPool");
    if (function == NULL) missing_symbol("ABinderProcess_startThreadPool");
    function();
}

void ABinderProcess_joinThreadPool(void) {
    void_fn function = (void_fn)binder_ndk_symbol("ABinderProcess_joinThreadPool");
    if (function == NULL) missing_symbol("ABinderProcess_joinThreadPool");
    function();
}

static const char* exception_name(int32_t exception) {
    switch (exception) {
        case 0: return "EX_NONE";
        case -1: return "EX_SECURITY";
        case -2: return "EX_BAD_PARCELABLE";
        case -3: return "EX_ILLEGAL_ARGUMENT";
        case -4: return "EX_NULL_POINTER";
        case -5: return "EX_ILLEGAL_STATE";
        case -6: return "EX_NETWORK_MAIN_THREAD";
        case -7: return "EX_UNSUPPORTED_OPERATION";
        case -8: return "EX_SERVICE_SPECIFIC";
        case -9: return "EX_PARCELABLE";
        case -129: return "EX_TRANSACTION_FAILED";
        default: return "EX_UNKNOWN";
    }
}

int32_t AServiceManager_addService(AIBinder* binder, const char* instance) {
    add_service_fn function =
            (add_service_fn)binder_ndk_symbol("AServiceManager_addService");
    is_declared_fn declared =
            (is_declared_fn)binder_ndk_symbol("AServiceManager_isDeclared");
    if (function == NULL) missing_symbol("AServiceManager_addService");
    const char* mode = getenv("FACEHAL_BINDER_MODE");
    bool system_test = mode != NULL && strcmp(mode, "system-test") == 0;
    int is_declared = declared != NULL ? declared(instance) : -1;
    FACEHAL_TRACE("FaceHAL register: instance=%s declared=%d mode=%s\n",
            instance, is_declared, system_test ? "system-test" : "vintf");
    // Only the top-level IFace object is downgraded, and only by explicit test opt-in.
    // Session and callback objects retain their generated VINTF contracts.
    if (system_test && strcmp(instance, "android.hardware.biometrics.face.IFace/default") == 0) {
        mark_vintf_stability_fn downgrade = (mark_vintf_stability_fn)
                binder_ndk_symbol("AIBinder_forceDowngradeToSystemStability");
        if (downgrade == NULL) missing_symbol("AIBinder_forceDowngradeToSystemStability");
        downgrade(binder);
    }
    int32_t exception = function(binder, instance);
    if (exception != 0) {
        fprintf(stderr, "FaceHAL registration error: instance=%s raw=%d declared=%d exception=%s\n",
                instance, exception, is_declared, exception_name(exception));
    }
    FACEHAL_TRACE("FaceHAL register: raw=%d hex=0x%08x exception=%s\n",
            exception, (uint32_t)exception, exception_name(exception));
    if (exception == -3 && is_declared == 0 && !system_test) {
        FACEHAL_TRACE("FaceHAL register: install the device VINTF IFace/default v4 fragment; a compatibility matrix is insufficient.\n");
    }
    return exception;
}

void AIBinder_setRequestingSid(AIBinder* binder, bool requesting_sid) {
    set_requesting_sid_fn function =
            (set_requesting_sid_fn)binder_ndk_symbol("AIBinder_setRequestingSid");
    if (function != NULL) function(binder, requesting_sid);
}

void AIBinder_markVintfStability(AIBinder* binder) {
    mark_vintf_stability_fn function =
            (mark_vintf_stability_fn)binder_ndk_symbol("AIBinder_markVintfStability");
    if (function == NULL) missing_symbol("AIBinder_markVintfStability");
    function(binder);
}

const char* AIBinder_Class_getDescriptor(const AIBinder_Class* clazz) {
    get_class_descriptor_fn function =
            (get_class_descriptor_fn)binder_ndk_symbol("AIBinder_Class_getDescriptor");
    return function != NULL ? function(clazz) : "";
}

AParcel* AParcel_create(void) {
    parcel_create_fn function = (parcel_create_fn)binder_ndk_symbol("AParcel_create");
    return function != NULL ? function() : NULL;
}

int32_t AParcel_getDataSize(const AParcel* parcel) {
    parcel_get_data_size_fn function = (parcel_get_data_size_fn)binder_ndk_symbol("AParcel_getDataSize");
    return function != NULL ? function(parcel) : -1;
}

binder_status_t AParcel_appendFrom(const AParcel* from, AParcel* to, int32_t start, int32_t size) {
    parcel_append_from_fn function = (parcel_append_from_fn)binder_ndk_symbol("AParcel_appendFrom");
    return function != NULL ? function(from, to, start, size) : -38;
}
