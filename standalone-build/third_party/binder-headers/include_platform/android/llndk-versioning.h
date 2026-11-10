// SPDX-License-Identifier: Apache-2.0
// Standalone NDK builds do not ship the platform LLNDK annotation header.
#pragma once
#include <android/versioning.h>
#ifndef __INTRODUCED_IN_LLNDK
#define __INTRODUCED_IN_LLNDK(api) __INTRODUCED_IN(api)
#endif
#ifndef __ANDROID_API_V__
#define __ANDROID_API_V__ 10000
#endif
