# 第三方内容与来源

本仓库基于 [LazyBonesLZY/facehal-rs](https://github.com/LazyBonesLZY/facehal-rs)。本次兼容分支的源码改动遵循根LICENSE的Apache-2.0，并保留上游作者与项目链接。

- `standalone-build/third_party/binder` 与Binder headers含AOSP版权和Apache许可头，原样保留。
- 已生成AIDL包含其生成说明、版本/hash及原始注释，原样保留。
- 上游已追踪的staging运行库保持原git blob内容，不以源码项目的Apache声明重新赋予其二进制授权。
- JNI/识别、TensorFlow Lite、GPU delegate、C++ runtime与平台Camera NDK来自不同组件；库文件名本身不足以识别具体构建来源、版本或再分发许可。

当前上游staging描述将prebuilts置于Apache默认元数据，但这不是对闭源/OEM runtime原许可的独立核验。本fork没有新增抽取的donor/device二进制；公开发布包含runtime的模块或镜像前，需按实际版本与来源核对相应许可与通知。源码构建与私有库打包接口不自动上传这些payload。

设备模板、录入样本、日志、凭据、序列号、私人网络地址和本机SDK路径不属于公开源树。`runtime/`、`out/`和本地配置默认忽略。
