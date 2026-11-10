# KSU 模块构建与使用

## 打包

先执行主README的Linux构建，准备与设备匹配的私有库目录，再运行：

```sh
python3 scripts/build-ksu-module.py \
  --hal out/android.hardware.biometrics.face-service.facehal \
  --bridge out/facehal-virtual-bridge \
  --probe out/iface-probe \
  --lib-dir runtime/private/lib64 \
  --output out/facehal-xiaomi-mtk.zip
```

当前私有库集合针对已测K50/ColorOS17栈。算法/JNI/TFLite/C++ runtime沿用对应原库；Camera NDK/AIDL库必须与系统服务匹配。打包器验证arm64 ELF、BOM/LF、CRC，并保持源树不写入payload。

## 运行方式

模块不提供全局system/vendor覆盖。`skip_mount`、进程专用LD_LIBRARY_PATH、固定算法线程和独立桥接进程构成测试运行栈。init/Soong正式设备集成仍需要device VINTF和平台策略，不能把System测试模式当成正式VINTF注册证明。

KSU可能把完整新模块放在 `/data/adb/modules_update/facehal_xiaomi_mtk`，active目录暂时只有update标记；此时Action显示与热启动可用性不同。需要无重启测试时可在root shell调用pending目录脚本，避免为了迁移目录消耗DSU会话。

```sh
sh /data/adb/modules_update/facehal_xiaomi_mtk/action.sh
sh /data/adb/modules_update/facehal_xiaomi_mtk/control.sh status
sh /data/adb/modules_update/facehal_xiaomi_mtk/bridge_control.sh probe
```

正常迁入active后路径为 `/data/adb/modules/facehal_xiaomi_mtk`。`service.sh`等boot_completed，最长120秒；管理器Action执行启动与探测。

## 配置与停止

- `mirror.conf`：1为自拍显示镜像，0为相机视角。只改变预览显示，不修改识别输入或已有模板。
- `mode.conf`：默认system-test用于已测移植栈；正常VINTF部署需选vintf并提供实际device manifest。
- 不同时启动多个提供IFace/default的模块。升级前先停止本模块两个进程，避免替换仍执行的文件影响PID身份校验。

```sh
sh /data/adb/modules_update/facehal_xiaomi_mtk/bridge_control.sh stop
sh /data/adb/modules_update/facehal_xiaomi_mtk/control.sh stop
```

stop只停当前进程；长期停用还需KSU管理器禁用模块。卸载入口只停止模块进程，保留既有模板。日志在模块logs目录，仅在设备本地保存。
