// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdio.h>
#ifndef FACEHAL_VERBOSE_LOGS
#define FACEHAL_VERBOSE_LOGS 0
#endif
// Keep arguments type-checked while optimized release builds remove their evaluation.
#define FACEHAL_TRACE(...) do { if (FACEHAL_VERBOSE_LOGS) fprintf(stderr, __VA_ARGS__); } while (0)
#define FACEHAL_ANDROID_TRACE(priority, tag, format, ...) \
    ((FACEHAL_VERBOSE_LOGS || (priority) >= 6) ? \
     __android_log_print(priority, tag, format, ##__VA_ARGS__) : 0)
