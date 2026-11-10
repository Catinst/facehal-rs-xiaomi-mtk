# 手动 Android CI 源码构建

这个工作流验证 host core/actor/preview 测试，并从仓库生产源码编译 AArch64 HAL 与两个 helper。
**产物是源码构建结果，加上模块文本模板；不是完整可刷设备模块，也不是硬件验证结论。**
CI 采用仅链接用符号桩弥补 runner 缺少设备 system/vendor 链接库的问题。桩没有业务行为，生产对象仍按正规 NDK 头文件编译。

## 跑一次后立即返回

工作流只有 `workflow_dispatch` 事件。push、pull request、定时任务都不会触发它。
先将工作流合入仓库默认分支，然后手动发送一次请求：

```sh
gh workflow run manual-android-build.yml --repo Catinst/facehal-rs-xiaomi-mtk --ref main -f verbose_logs=false
```

这条命令发送 dispatch 后即返回；无需等待 runner。任务约定不执行 `gh run watch`、轮询或查看任何 run。
需要详细运行日志时，可在同一条命令中将 `verbose_logs` 设为 `true`。

| 设置 | 值 | 目的 |
| --- | --- | --- |
| Runner | `ubuntu-24.04` | Linux NDK 工具链与 host 测试 |
| 权限 | `contents: read` | 工作流没有 GitHub 写权限 |
| Checkout | `persist-credentials: false` | 不在 checkout 中保留访问凭据 |
| Rust | stable，`aarch64-linux-android` | Cargo 构建使用已有 lockfile 与 `--locked` |
| NDK | 官方 r27c，`27.2.12479018` | 用 runner 官方 SDK 的 `sdkmanager` 安装 |
| Host 工具 | `g++`、`libclang-18-dev` | actor/preview 测试、Binder bindgen |
| 最低 API | `FACEHAL_MIN_SDK=31` | 与本 CI 链接契约一致 |
| 详细日志 | 布尔输入，默认 false | 映射为 `FACEHAL_VERBOSE_LOGS=0`；true 映射为 1 |

输入只经 GitHub 布尔表达式映射成固定 `0`/`1` 环境变量，不拼入 shell 命令。
checkout/upload-artifact 固定完整 commit SHA。安装步骤使用官方 SDK/NDK；不访问设备、不下载 donor 库。
工作流自身没有 dispatch、push、release 或 GitHub API 写操作。

## 本地复现构建环境

在新鲜的 Ubuntu 24.04 checkout 中，先准备官方 Android SDK command-line tools，再执行：

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends -y g++ libclang-18-dev
rustup toolchain install stable --profile minimal --target aarch64-linux-android
rustup default stable
"$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$ANDROID_HOME" 'ndk;27.2.12479018'
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
export LIBCLANG_PATH=/usr/lib/llvm-18/lib
export FACEHAL_VERBOSE_LOGS=0
python3 -B -m unittest discover -s ci -p 'test_*.py' -v
sh scripts/test-compat.sh
bash ci/build.sh
```

测试入口 `scripts/test-compat.sh` 运行 locked Cargo core 测试、`thread_owner_test.cpp` 的 actor 线程归属测试和 `preview_geometry_test.cpp` 的旋转/镜像几何测试。
这些是 host fixtures，和 Android production ELFs 严格分离。
`ci/build.sh` 调用已有 `scripts/build-production.sh`，产出真实生产 HAL 和 helper，不以 dummy main 替换业务源码。

## 缺少设备库时如何链接

`ci/prepare_link_stubs.py` 固定验证 NDK revision 与 API 31，然后读取官方 NDK `.so` 的动态已定义外部导出。
`llvm-nm --dynamic --defined-only --extern-only --format=posix` 提取符号，去掉符号版本后缀和绝对版本标签。
它写入以下 SONAME 对应的独立共享桩：

| 桩 SONAME | 符号来源 | 附加符号 |
| --- | --- | --- |
| `libcamera2ndk_vendor.so` | NDK API 31 `libcamera2ndk.so` public exports | 无设备二进制来源 |
| `libmediandk.so` | NDK API 31 `libmediandk.so` exports | `AImageReader_getWindowNativeHandle` |
| `libnativewindow.so` | NDK API 31 `libnativewindow.so` exports | `ANativeWindow_readFromParcel` |
| `libbinder_ndk.so` | NDK API 31 `libbinder_ndk.so` exports | vendored `binder-headers/include_ndk` 和 `include_platform` 中的 Binder 函数名，包括 `ABinderProcess`、`AIBinder`、`AParcel`、`AServiceManager`；并补上 `AIBinder_Class_setTransactionCodeToFunctionNameMap` |

桩内每个符号只对应 `void symbol(void) { __builtin_trap(); }`；它们不表达真实函数签名，也没有 API/业务实现。
**桩只让链接器找到符号，绝不执行、安装或打包。** 每个桩含独立 section `.facehal_ci_link_only` 和字符串标识 `FACEHAL_CI_LINK_ONLY_DO_NOT_SHIP_OR_EXECUTE`。
生成后再次验证导出集合、SONAME、标识以及无 `DT_NEEDED`，并将来源路径、NDK 输入库 SHA-256 和附加符号写入私有 `manifest.json`。

`runtime/ci-link-stubs/lib64/` 还包含官方 NDK 的 `libc++_shared.so` 拷贝，仅提供给链接器，同样排除在上传内容之外。
脚本导出：

```sh
export ANDROID_SYSTEM_LIB64="$PWD/runtime/ci-link-stubs/lib64"
export ANDROID_VENDOR_LIB64="$ANDROID_SYSTEM_LIB64"
export BINDER_NDK_LIB_DIR="$ANDROID_SYSTEM_LIB64"
export FACEHAL_MIN_SDK=31
```

生产 C/C++ 对象继续用 NDK C/C++ 头文件；Binder 使用已有 vendored platform 声明和正规的 NDK/bindgen 编译路径。
既有 Binder compatibility trampoline 保持原行为，不会把桩函数链接为静态业务实现。
共享桩的正常 SONAME 可出现在最终 ELF `DT_NEEDED` 中；这只表示设备运行时必须解析到匹配的真实库，不代表桩内容进入了可执行文件。
脚本不设置桩目录为 `LD_LIBRARY_PATH`，不运行任何 Android target ELF。
生成目录要求初始不存在，避免复用残留设备库或旧桩；重复构建请使用新 checkout。

## 上传内容的精确边界

`ci/stage_artifact.py` 在构建成功后验证三份 ELF 的 ELF64、小端、AArch64、可执行/PIE 类型与动态依赖；拒绝桩标识、RPATH/RUNPATH 和含路径的 `DT_NEEDED`。
模块模板只接受 UTF-8、LF、无 BOM、无 NUL 的文本文件，拒绝符号链接。
产物从固定白名单分级到 `out/ci-artifact/`，工作流只上传这个目录：

```text
out/ci-artifact/
  out/android.hardware.biometrics.face-service.facehal
  out/facehal-virtual-bridge
  out/iface-probe
  module/module.prop
  module/control.sh
  module/bridge_control.sh
  module/service.sh
  module/action.sh
  module/customize.sh
  module/uninstall.sh
  module/mode.conf
  module/mirror.conf
  module/skip_mount
  module/reference/device-manifest.xml
  docs/ci.md
  runtime-dependencies.md
```

`runtime-dependencies.md` 记录这三份 ELF 的 SHA-256、直接 `DT_NEEDED` 以及另行满足的运行依赖。
只有三份生产 ELF；其余文件全是文本。没有任何 `.so`、host 测试程序、NDK 文件、链接桩或 device/donor 二进制。
`runtime/`、`standalone-build/target/`、`out/tests/` 和整个仓库都不会被上传。
artifact 保留 7 天，不自动发布 release，不生成设备模块 ZIP。

## 完成真实设备模块还缺什么

要按 `scripts/build-ksu-module.py` 组装完整设备模块，需要匹配目标系统/vendor 的真实运行库，而非 CI 桩：

- 真实 `libcamera2ndk_vendor.so`，以及设备提供的 media/nativewindow/Binder NDK 实现。
- 兼容的 `libc++_shared.so`、`libfacehal_jni.so`。
- 算法运行库 `libjni_stfaceunlock_api.so`、`libtensorflowlite.so`、`libtensorflowlite_gpu_delegate.so`。
- `android.frameworks.cameraservice.common-V1-ndk.so`、`android.frameworks.cameraservice.device-V3-ndk.so`、`android.frameworks.cameraservice.service-V3-ndk.so`。
- `android.hardware.common-V2-ndk.so`、`android.hardware.common.fmq-V1-ndk.so`。

部分库通过 `dlopen` 或间接依赖加载，直接 `DT_NEEDED` 并非完整运行依赖表。
需独立验证 Binder platform API 的实际可用性、Camera vendor 扩展、算法 ABI、链接器 namespace、VINTF、SELinux、启动与硬件行为。
API 31 是编译目标，不承诺任意 Android/ROM/SoC 可运行。
成功的 CI 源码构建只证明相应测试与编译链接完成；设备部署和硬件认证仍是后续工作。
