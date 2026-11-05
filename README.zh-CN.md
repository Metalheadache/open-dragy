<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- Copyright 2026 Yizhan Wu -->
# Open Dragy

Dragy–ESP32 工程参考

[English](README.md)

本独立工程参考介绍：如何通过 ESP32 的 BLE 接收兼容 Dragy 设备的 GPS 遥测数据，如何解码和转换数据、验证接收机输出频率，以及如何计算实验性的速度与加速、距离、制动成绩。内容包括可移植的 C++17 核心、最小 ESP-IDF 串口示例、合成数据回放工具及测试。

## 内容范围

| 包含的方向 | 参考实现 |
| --- | --- |
| Dragy → ESP32 GPS 数据接收 | 基于已观察行为的 FD00/FD02/FD03 握手、CCCD 回读、有界通知队列和连接恢复 |
| GPS 坐标及转换 | UBX NAV-PVT 解码、有符号整数与单位转换、局部东向/北向投影及逆变换 |
| 速度与直线性能计时 | 接收机地速、速度/距离阈值插值、静止起步及制动停车时刻估算 |
| 25 Hz 输出 | 可选的 DRG71 专用 RAM 配置请求；通过接收机时间戳独立验证 40 ms 间隔 |
| 优化方法 | 固定大小缓冲区、简短回调、独立消费任务及可复现的解析性能测试 |
| 数据一致性 | 校验和、连接会话隔离、丢失边界、接收机时间连续性、重复包、周回绕及过期/突发交付处理 |

本包仅包含以上方法及其必要示例、工具、测试和文档。不包含产品应用代码、触屏界面、美术资源、板级显示/触控/电源驱动、设置服务、持久化 PB/历史记录、圈速/分段计时、赛道数据库、路线管理、存储恢复、更新工具或生产固件镜像。ESP32 示例仅是通过串口输出的开发应用，不是完整仪表固件。

## 数据处理流程

- 兼容性来自已观察的协议行为，并非官方 Dragy SDK。可选频率请求仅面向已明确识别为 `DRG71` 的设备；未知型号保留原数据流。
- 解析器仅接受通过校验的 100 字节 UBX NAV-PVT 帧，其有效载荷为 92 字节。单次 BLE 通知可以只包含半帧，也可以包含多帧。
- 坐标由接收机定位结果解码得到；本参考不求解卫星导航定位。局部坐标投影属于近似方法，不是全球测地算法，也不是坐标基准转换。
- 速度采用接收机水平地速。计时使用接收机 GPS 周内时间；主机单调时间用于检查数据是否过期及交付是否异常。可选的静止速度显示滤波不参与性能计时。
- 性能计时为实验性方法，采用零 rollout（不扣除起步移动距离）和明确的质量、连续性门限。不实现官方 Dragy 成绩验证、坡度修正或道路坡度核验。
- 25 Hz 表示接收机采样间隔为 40 ms；阈值插值使用相邻的接收机采样时刻。

本项目是独立工程参考，与 Dragy、u-blox 或 Espressif 无隶属或背书关系，也不实现其专有软件。相关名称仅用于说明兼容性及技术来源。

## 桌面构建与回放

需要 C++17 编译器、CMake 3.20 或更新版本；包审计需要 Python 3.9 或更新版本。桌面核心不依赖任何产品固件源码树或第三方运行库。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
build/reference_generate build/synthetic.csv
build/reference_replay build/synthetic.csv --arm --expect-complete
build/reference_benchmark
python3 tools/audit_package.py
```

Clang/GCC 下默认启用 AddressSanitizer 和 UndefinedBehaviorSanitizer。单独进行优化后的性能测试时，可设置 `-DREFERENCE_SANITIZERS=OFF`。测试始终启用断言。生成的 CSV 包含合成的分片数据包及交付时间戳；回放按记录的时间戳计算，不执行实时等待，也不以回放程序运行时间代替数据交付时间。详见[回放格式](docs/06-consistency.md)。

## ESP32 示例

依赖版本为 ESP-IDF **5.4.2** 和 `h2zero/esp-nimble-cpp` **2.3.4**；目标为 ESP32-S3。组件管理器会下载依赖源码，本包不附带这些依赖。

```sh
# 先激活 ESP-IDF 5.4.2 环境。
cd examples/esp32
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
```

在 **Dragy engineering reference** 菜单中，将设备地址留空时仅扫描并列出候选设备，不建立连接。连接前需选择接收机准确的 BLE 地址及地址类型。不要提交包含私有信息的本地配置。25 Hz 请求和性能计时布防是独立的可选设置，默认均关闭。连接流程详见[数据接收指南](docs/01-retrieval.md)。

## 工程说明

1. [数据接收及最小 ESP32 适配器](docs/01-retrieval.md)
2. [GPS 字段和坐标转换](docs/02-coordinates.md)
3. [速度与性能计时](docs/03-timing.md)
4. [25 Hz 请求与验证](docs/04-rate.md)
5. [优化方法及性能测试](docs/05-optimization.md)
6. [数据一致性及回放](docs/06-consistency.md)

另见[测试说明](docs/TESTING.md)、[来源署名](NOTICE)和[许可说明](docs/LICENSING.md)。

## 许可证

Copyright 2026 Yizhan Wu。本项目采用 [MPL-2.0](LICENSE)。[NOTICE](NOTICE) 中标明的两个 BLE 文件同时保留适用的 Apache-2.0 条款，详见[逐文件许可清单](licensing-map.json)。
