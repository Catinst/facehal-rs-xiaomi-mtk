# facehal-rs-xiaomi-mtk

基于 [LazyBonesLZY/facehal-rs](https://github.com/LazyBonesLZY/facehal-rs) 的真实 fork，聚焦 **小米/Redmi MTK 设备在新系统、Treble/GSI 环境下的人脸 HAL 兼容**。上游署名、Apache-2.0 源码许可与第三方归属保留。

**目前只明确实测 Redmi K50（rubens/MT6895）＋ColorOS17 SDK37＋vendor/VNDK31：人脸录入、完成页和锁屏解锁。其它小米 MTK 机型是适配目标，尚未逐机型验证；不是“全部 MTK 已通用”。**

## 这个 fork 改了什么

- [算法所有者线程](<native/AlgorithmThreadBridge.cpp>)：初始化、推理、元数据、删除/重置与释放在同一个线程；解决 GPU delegate 初始化/执行线程不一致。
- [真实 VirtualHal 桥接](<native/FaceVirtualBridge.c>)：不稳定接口 `IVirtualHal/virtual` 的事务23返回真实 `IFace/default`，不生成虚拟认证结果。
- [预览](<native/PreviewBridge.cpp>)：传感器方向旋转、等比居中裁剪、最长边640的显示缓冲；`FACEHAL_PREVIEW_MIRROR=1` 只水平翻转显示，原识别输入和模板保持。
- [标准录入进度](<rust/service/src/session.rs>)：真实候选和四次后续匹配确认发送标准 `4→3→2→1→0`，替代 MIUI 私有提示；最终0只在真实提交后发一次。
- [Binder 诊断](<standalone-build/third_party/binder/sys/binder_ndk_compat.c>)：保留原始 exception 与声明状态；正常模式仍使用 VINTF，显式 `system-test` 才对根对象降级用于兼容测试。
- [KSU 模板](<module/module.prop>)：私有库、开机完成后启动、启动/停止/探测、进程身份检查；不覆盖全局库。

## 硬件链路

```text
设置 / 锁屏 → 系统 FaceService / FaceProvider
                  ↓
       可选 VirtualHal → 真实 Face AIDL v4
                  ↓
         facehal-rs Rust/C++ 服务
          ├─ 算法：固定线程 → 原算法运行库
          └─ 相机：Camera2 NDK → 系统 cameraserver
                                → 设备原 vendor camera provider → 原内核驱动
```

本项目不是替换 K50 摄像头驱动，也不是重写/训练识别模型。返回 WEAK/RGB 是能力声明，不等于已完成强度认证；未把它升级成硬件强认证或宣称密码学信任链已验证。

## Linux 构建

使用 Linux/WSL、Rust（支持 lockfile v4）、Android NDK、libclang；测试环境采用 NDK r27c。实际运行还依赖目标系统配套库，编译最低API不等于全版本运行兼容。

```sh
rustup target add aarch64-linux-android
export ANDROID_NDK_HOME="$HOME/android-ndk-r27c"
export ANDROID_SYSTEM_LIB64="$PWD/runtime/system/lib64"
export ANDROID_VENDOR_LIB64="$PWD/runtime/vendor/lib64"
export LIBCLANG_PATH=/usr/lib/llvm-18/lib
export FACEHAL_MIN_SDK=31
scripts/build-production.sh
```

`runtime/` 由使用者准备匹配设备的库，默认忽略。正常构建使用锁文件并可获取依赖；已缓存 crates 时可设 `FACEHAL_OFFLINE=1`。输出在 `out/`：HAL、VirtualHal 桥接和接口探测器。此路径不包含新抽取的系统/vendor 二进制。

## KSU 打包与设备验证

构建输出与所需私有运行库由 [打包脚本](<scripts/build-ksu-module.py>) 组装；模板在 [module/](<module/module.prop>)。详见 [模块说明](<docs/module.md>) 和 [设备支持记录](<docs/devices.md>)。

- Camera NDK/AIDL库要匹配系统服务版本，按当前设备 linker namespace 验证。
- 基础 binder/foundation/fmq/camera_metadata 优先使用目标当前系统配套版本；原库不被全局替换。
- 正式设备集成仍需正确 device VINTF、init、标签与策略。System测试模式不等于正式VINTF通过。
- KSU pending 安装与整机重启后自动加载是不同验证阶段。

## 测试

```sh
scripts/test-compat.sh
```

测试覆盖 core 回归、固定线程/异常传递，以及实际预览实现的旋转、水平翻转和输入不变；假后端与假窗口仅在测试链接，不进生产模块。硬件录入、完成页、锁屏和重启需分别实机验收。

## 许可与归属

第一方源码遵循 [LICENSE](<LICENSE>) 的 Apache-2.0。上游 AOSP 版权/许可头、生成 AIDL 注释和第三方归属保留；Binder 兼容实现的修改在 fork 差异中明确记录。上游已追踪的运行库保持原内容；这些库仍受各自原许可约束，项目 Apache 说明不替代其二进制许可。本 fork 不新增上传从设备或 donor 抽取的库、模板、日志、凭据或本机配置。详见 [第三方说明](<docs/third-party.md>)。
