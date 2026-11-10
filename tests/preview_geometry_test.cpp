// SPDX-License-Identifier: Apache-2.0
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../native/PreviewBridge.cpp"
int main() {
    const int width = 8, height = 6;
    std::vector<uint8_t> frame(width * height * 3 / 2, 128);
    for (int i = 0; i < width * height; ++i) frame[i] = static_cast<uint8_t>(32 + i * 3);
    const auto original = frame;
    for (int orientation : {0, 90, 180, 270}) {
        int outW = orientation == 90 || orientation == 270 ? height : width;
        int outH = orientation == 90 || orientation == 270 ? width : height;
        std::vector<uint8_t> normal(outW * outH * 4), mirrored(normal.size());
        ANativeWindow_Buffer a{outW, outH, outW, WINDOW_FORMAT_RGBA_8888, normal.data()};
        ANativeWindow_Buffer b{outW, outH, outW, WINDOW_FORMAT_RGBA_8888, mirrored.data()};
        setenv("FACEHAL_PREVIEW_MIRROR", "0", 1);
        assert(writeBuffer(a, frame.data(), frame.size(), width, height, orientation));
        setenv("FACEHAL_PREVIEW_MIRROR", "1", 1);
        assert(writeBuffer(b, frame.data(), frame.size(), width, height, orientation));
        for (int y = 0; y < outH; ++y) for (int x = 0; x < outW; ++x)
            for (int channel = 0; channel < 4; ++channel)
                assert(mirrored[(y * outW + x) * 4 + channel] ==
                       normal[(y * outW + outW - 1 - x) * 4 + channel]);
        assert(frame == original);
    }
    ANativeWindow window;
    assert(facehal_preview_render_nv21(&window, frame.data(), frame.size(), width, height, 270, 1) == 0);
    assert(window.width == 360 && window.height == 640);
    assert(frame == original);
    std::puts("PREVIEW_TEST PASS rotations=4 mirror=horizontal source=unchanged bounded=640");
}
