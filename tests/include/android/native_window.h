// SPDX-License-Identifier: Apache-2.0
// Host-only test double; not part of Android builds.
#pragma once
#include <cstdint>
#include <vector>
constexpr int WINDOW_FORMAT_RGBA_8888 = 1;
constexpr int WINDOW_FORMAT_RGBX_8888 = 2;
constexpr int WINDOW_FORMAT_RGB_565 = 4;
struct ANativeWindow_Buffer {
    int32_t width, height, stride, format;
    void* bits;
};
struct ANativeWindow {
    int defaultWidth = 1080, defaultHeight = 1920;
    int width = 1080, height = 1920, format = 1;
    std::vector<uint8_t> pixels;
};
inline int ANativeWindow_setBuffersGeometry(ANativeWindow* w, int width, int height, int format) {
    w->width = width == 0 ? w->defaultWidth : width;
    w->height = height == 0 ? w->defaultHeight : height;
    w->format = format;
    return 0;
}
inline int ANativeWindow_getWidth(ANativeWindow* w) { return w->width; }
inline int ANativeWindow_getHeight(ANativeWindow* w) { return w->height; }
inline int ANativeWindow_lock(ANativeWindow* w, ANativeWindow_Buffer* b, void*) {
    w->pixels.assign(static_cast<size_t>(w->width) * w->height * 4, 0);
    *b = {w->width, w->height, w->width, w->format, w->pixels.data()};
    return 0;
}
inline int ANativeWindow_unlockAndPost(ANativeWindow*) { return 0; }
