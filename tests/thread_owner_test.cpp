#include "SenseTimeBridge.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <thread>

struct FacehalAlgorithm {};
static std::thread::id owner;
static std::atomic<int> calls{0};
static void verify() { assert(std::this_thread::get_id() == owner); ++calls; }
extern "C" FacehalAlgorithm* facehal_algorithm_open_impl(const char*) {
    owner = std::this_thread::get_id(); ++calls; return new FacehalAlgorithm;
}
extern "C" void facehal_algorithm_close_impl(FacehalAlgorithm* p) { verify(); delete p; }
extern "C" int32_t facehal_algorithm_process_nv21_impl(FacehalAlgorithm*, const uint8_t* p,
        size_t n, int32_t, int32_t, int32_t, int32_t, int32_t) {
    verify(); assert(n == 6 && p[0] == 7); return -71;
}
extern "C" int32_t facehal_algorithm_current_face_id_impl(const FacehalAlgorithm*) { verify(); return -1; }
extern "C" int32_t facehal_algorithm_enumerate_impl(const FacehalAlgorithm*, int32_t*, size_t) { verify(); return 0; }
extern "C" int32_t facehal_algorithm_slots_impl(const FacehalAlgorithm*, int32_t* p, size_t n) {
    verify(); assert(n == 2); p[0] = p[1] = -1; return 2;
}
extern "C" int32_t facehal_algorithm_remove_impl(FacehalAlgorithm*, int32_t) { verify(); return 0; }
extern "C" int32_t facehal_algorithm_remove_all_impl(FacehalAlgorithm*) { verify(); return 0; }
extern "C" int32_t facehal_algorithm_set_active_slot_impl(FacehalAlgorithm*, int32_t slot) {
    verify(); if (slot == -9) throw std::runtime_error("fixture exception"); return 0;
}
extern "C" int32_t facehal_algorithm_reset_impl(FacehalAlgorithm*) { verify(); return 0; }
extern "C" void facehal_algorithm_set_orientation_impl(FacehalAlgorithm*, int32_t) { verify(); }

int main() {
    const auto caller = std::this_thread::get_id();
    FacehalAlgorithm* p = facehal_algorithm_open("fixture-only");
    assert(p && owner != caller);
    auto exercise = [p] {
        uint8_t frame[6] = {7, 0, 0, 0, 128, 128};
        int32_t ids[2];
        assert(facehal_algorithm_process_nv21(p, frame, 6, 2, 2, 2, 1, 0) == -71);
        assert(facehal_algorithm_current_face_id(p) == -1);
        assert(facehal_algorithm_enumerate(p, ids, 2) == 0);
        assert(facehal_algorithm_slots(p, ids, 2) == 2);
        assert(facehal_algorithm_remove(p, 1) == 0);
        assert(facehal_algorithm_remove_all(p) == 0);
        assert(facehal_algorithm_set_active_slot(p, 0) == 0);
        assert(facehal_algorithm_reset(p) == 0);
        facehal_algorithm_set_orientation(p, 270);
    };
    std::thread a(exercise), b(exercise);
    a.join(); b.join();
    assert(facehal_algorithm_set_active_slot(p, -9) == -4);
    assert(facehal_algorithm_reset(p) == 0);
    facehal_algorithm_close(p);
    assert(calls.load() >= 20);
    std::printf("OWNER_THREAD_TEST PASS calls=%d\n", calls.load());
}
