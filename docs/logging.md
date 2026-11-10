# 日志与发布构建

当前测试模块的逐帧信息主要来自HAL二进制（Rust及native C/C++）；KSU脚本负责将stderr/stdout接到模块文件，同时手动Action/探测器会回显进程和接口结果。

仓库默认发布构建为 `FACEHAL_VERBOSE_LOGS=0`：项目例行/逐帧诊断在编译时关闭。项目 Rust/native 错误直接进入 Android logcat，不通过运行日志文件；默认标签 `FaceHAL`，个人构建可设置 `FACEHAL_LOG_TAG=SnowColorOSFaceHal`。第三方算法库自身stderr行为不改写；最终项目错误代码仍由logcat报告。显式 `FACEHAL_VERBOSE_LOGS=1` 可编译诊断版本，诊断也进入logcat。

```sh
FACEHAL_VERBOSE_LOGS=0 scripts/build-production.sh
FACEHAL_VERBOSE_LOGS=1 scripts/build-production.sh
```

模块启动/探测成功改为真实接口验证，不依赖“注册成功日志”出现，安静版不会因此误判启动超时。手动Action的探测回显属于人工诊断，不是后台逐帧日志。当前已经安装的个人测试模块保持原样；本轮只改变仓库后续发布构建默认值。

Actions手动构建输入verbose_logs默认false。编译器日志与CI测试报告仍出现在Actions控制台；它们与手机运行日志是两回事。CI产物只包含源码编译的可执行文件和模板，链接桩不会进下载包；完整设备模块仍需匹配运行库。
