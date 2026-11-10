// SPDX-License-Identifier: Apache-2.0
#pragma once
#ifndef FACEHAL_VERBOSE_LOGS
#define FACEHAL_VERBOSE_LOGS 0
#endif
#ifndef FACEHAL_LOG_TAG
#define FACEHAL_LOG_TAG "FaceHAL"
#endif
#ifdef __ANDROID__
#include <android/log.h>
#define FACEHAL_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, FACEHAL_LOG_TAG, __VA_ARGS__)
#define FACEHAL_TRACE(...) do { if (FACEHAL_VERBOSE_LOGS) __android_log_print(ANDROID_LOG_INFO, FACEHAL_LOG_TAG, __VA_ARGS__); } while (0)
#define FACEHAL_ANDROID_TRACE(priority, tag, format, ...) \
    ((void)(tag), (FACEHAL_VERBOSE_LOGS || (priority) >= ANDROID_LOG_ERROR) ? \
     __android_log_print(priority, FACEHAL_LOG_TAG, format, ##__VA_ARGS__) : 0)
#else
#include <stdio.h>
#define FACEHAL_ERROR(...) fprintf(stderr, __VA_ARGS__)
#define FACEHAL_TRACE(...) do { if (FACEHAL_VERBOSE_LOGS) fprintf(stderr, __VA_ARGS__); } while (0)
#endif
