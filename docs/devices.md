# 设备验证记录

## 明确已实测

| 项目 | 记录 |
|---|---|
| 设备 | Redmi K50，rubens，MT6895 |
| 系统 | ColorOS17 / Android SDK37，Treble/DSU环境 |
| vendor ABI | SDK31 / VNDK31 |
| 相机提供者 | 设备原MTK provider 2.6/internal/0，兼容2.4/2.5 |
| 采集 | 标准Camera2通路，前摄ID1，640x480 NV21，方向270度 |
| 已验证 | HAL注册、VirtualHal桥接、sensor发现、GPU线程、录入、完成页、锁屏解锁 |
| 预览 | 等比裁剪；显示水平翻转是独立可配置选项 |

这些是该设备/该移植栈的结果。其它小米/Redmi MTK机型尚需确认其provider、相机ID/方向、Camera2能力、AIDL版本、库ABI和系统FaceService配置；本仓库不把候选机型列为已通过。

## 逐机型验收

1. 记录系统SDK、vendor/VNDK与相机provider，不用系统品牌字符串推断驱动归属。
2. 静态解析ELF依赖，并核对实际linker namespace与同名基础库版本。
3. 验证Face接口版本/hash/属性、框架sensor发现和Session/close。
4. 确认算法初始化与处理同线程，相机可获得有效NV21帧。
5. 分别测试录入进度/完成页、取消/删除、暗光/不同姿态、锁屏解锁、锁定状态和重启。
6. 保留本机日志用于诊断，公开提交去除个人数据、真实模板、凭据与设备标识。

实际返回WEAK/RGB不代表已经完成生物强度、活体、防伪或密码学信任链认证；本项目不把它宣称为硬件强认证。
