#include "FacehalLog.h"
#define facehal_algorithm_open facehal_algorithm_open_impl
#define facehal_algorithm_close facehal_algorithm_close_impl
#define facehal_algorithm_process_nv21 facehal_algorithm_process_nv21_impl
#define facehal_algorithm_current_face_id facehal_algorithm_current_face_id_impl
#define facehal_algorithm_enumerate facehal_algorithm_enumerate_impl
#define facehal_algorithm_slots facehal_algorithm_slots_impl
#define facehal_algorithm_remove facehal_algorithm_remove_impl
#define facehal_algorithm_remove_all facehal_algorithm_remove_all_impl
#define facehal_algorithm_set_active_slot facehal_algorithm_set_active_slot_impl
#define facehal_algorithm_reset facehal_algorithm_reset_impl
#define facehal_algorithm_set_orientation facehal_algorithm_set_orientation_impl
#include "SenseTimeBridge.h"
#undef facehal_algorithm_open
#undef facehal_algorithm_close
#undef facehal_algorithm_process_nv21
#undef facehal_algorithm_current_face_id
#undef facehal_algorithm_enumerate
#undef facehal_algorithm_slots
#undef facehal_algorithm_remove
#undef facehal_algorithm_remove_all
#undef facehal_algorithm_set_active_slot
#undef facehal_algorithm_reset
#undef facehal_algorithm_set_orientation

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
class AlgorithmOwner {
 public:
    AlgorithmOwner() : worker_([this] { run(); }) {}
    ~AlgorithmOwner() {
        { std::lock_guard<std::mutex> lock(mutex_); stopped_ = true; }
        ready_.notify_one();
        worker_.join();
    }
    template<class Function>
    auto call(Function function) -> decltype(function()) {
        using Result = decltype(function());
        auto task = std::make_shared<std::packaged_task<Result()>>(std::move(function));
        auto result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            work_.emplace_back([task] { (*task)(); });
        }
        ready_.notify_one();
        // Borrowed frame/template pointers remain valid until the owning thread finishes.
        return result.get();
    }
 private:
    void run() {
        FACEHAL_TRACE("FaceHAL algorithm owner tid=%ld\n", syscall(SYS_gettid));
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                ready_.wait(lock, [this] { return stopped_ || !work_.empty(); });
                if (stopped_ && work_.empty()) return;
                task = std::move(work_.front());
                work_.pop_front();
            }
            task();
        }
    }
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> work_;
    bool stopped_ = false;
    std::thread worker_;
};
AlgorithmOwner& owner() { static AlgorithmOwner instance; return instance; }
template<class Function, class Result>
Result invoke(Function function, Result failure) {
    try { return owner().call(std::move(function)); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "FaceHAL algorithm dispatch error: %s\n", error.what());
        return failure;
    }
}
template<class Function>
void invokeVoid(Function function) {
    try { owner().call(std::move(function)); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "FaceHAL algorithm dispatch error: %s\n", error.what());
    }
}
}

extern "C" FacehalAlgorithm* facehal_algorithm_open(const char* directory) {
    return invoke([=] {
        FACEHAL_TRACE("FaceHAL algorithm open tid=%ld\n", syscall(SYS_gettid));
        return facehal_algorithm_open_impl(directory);
    }, static_cast<FacehalAlgorithm*>(nullptr));
}
extern "C" void facehal_algorithm_close(FacehalAlgorithm* algorithm) {
    invokeVoid([=] { facehal_algorithm_close_impl(algorithm); });
}
extern "C" int32_t facehal_algorithm_process_nv21(
        FacehalAlgorithm* algorithm, const uint8_t* image, size_t size, int32_t width,
        int32_t height, int32_t stride, int32_t action, int32_t config) {
    return invoke([=] {
        static int logged = 0;
        if (logged++ < 3) FACEHAL_TRACE("FaceHAL algorithm process tid=%ld\n", syscall(SYS_gettid));
        return facehal_algorithm_process_nv21_impl(algorithm, image, size, width, height, stride, action, config);
    }, int32_t{FACEHAL_ALGORITHM_ERROR_INITIALIZATION});
}
extern "C" int32_t facehal_algorithm_current_face_id(const FacehalAlgorithm* algorithm) {
    return invoke([=] { return facehal_algorithm_current_face_id_impl(algorithm); }, int32_t{-1});
}
extern "C" int32_t facehal_algorithm_enumerate(const FacehalAlgorithm* algorithm, int32_t* ids, size_t size) {
    return invoke([=] { return facehal_algorithm_enumerate_impl(algorithm, ids, size); }, int32_t{-4});
}
extern "C" int32_t facehal_algorithm_slots(const FacehalAlgorithm* algorithm, int32_t* ids, size_t size) {
    return invoke([=] { return facehal_algorithm_slots_impl(algorithm, ids, size); }, int32_t{-4});
}
extern "C" int32_t facehal_algorithm_remove(FacehalAlgorithm* algorithm, int32_t id) {
    return invoke([=] { return facehal_algorithm_remove_impl(algorithm, id); }, int32_t{-4});
}
extern "C" int32_t facehal_algorithm_remove_all(FacehalAlgorithm* algorithm) {
    return invoke([=] { return facehal_algorithm_remove_all_impl(algorithm); }, int32_t{-4});
}
extern "C" int32_t facehal_algorithm_set_active_slot(FacehalAlgorithm* algorithm, int32_t slot) {
    return invoke([=] { return facehal_algorithm_set_active_slot_impl(algorithm, slot); }, int32_t{-4});
}
extern "C" int32_t facehal_algorithm_reset(FacehalAlgorithm* algorithm) {
    return invoke([=] { return facehal_algorithm_reset_impl(algorithm); }, int32_t{-4});
}
extern "C" void facehal_algorithm_set_orientation(FacehalAlgorithm* algorithm, int32_t orientation) {
    invokeVoid([=] { facehal_algorithm_set_orientation_impl(algorithm, orientation); });
}
