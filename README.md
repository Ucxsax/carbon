# Carbon - Minecraft NPU 加速模组

PC 端 Minecraft NPU 异构算力加速模组。通过调用 PC 端硬件 NPU 分流、分担 Minecraft 原有 CPU、GPU 计算负载。

## 快速安装

**下载一个 JAR，丢进 mods/ 文件夹，完事。**

1. 安装 [Fabric Loader](https://fabricmc.net/)（支持 Minecraft 1.20.x）
2. 下载 [最新版本](https://github.com/Ucxsax/carbon/releases) 中的 `fabric-mod-0.1.0.jar`
3. 放入 `.minecraft/mods/` 文件夹
4. 启动游戏，首次运行自动释放原生库到 `.minecraft/carbon/native/`

> 所有依赖（ONNX Runtime、carbon_npu.dll、ONNX 模型）均已打包在 JAR 内，无需手动复制任何 DLL。

## 项目结构

```
carbon/
├── include/                        # C++ 头文件
│   └── carbon/
│       ├── common_types.h          # 公共类型定义
│       ├── scheduler/              # 任务调度层
│       ├── hal/                    # 硬件抽象层
│       ├── backends/               # 硬件后端
│       │   ├── ort_backend.h       # ONNX Runtime 后端
│       │   ├── openvino_backend.h  # Intel OpenVINO 后端
│       │   ├── directml_backend.h  # DirectML 后端
│       │   ├── tensorrt_backend.h  # NVIDIA TensorRT 后端
│       │   └── mock_backend.h      # 模拟后端（测试用）
│       ├── compute/                # NPU 计算处理器
│       └── utils/                  # 工具类
├── src/                            # C++ 源码
│   ├── backends/ort_backend.cpp    # ONNX Runtime 后端实现
│   ├── compute/                    # 计算处理器实现
│   ├── hal/                        # 硬件抽象层实现
│   ├── scheduler/                  # 调度器实现
│   └── utils/                      # 工具类实现
├── jni/                            # JNI 桥接层（Java <-> C++）
│   └── carbon_npu_jni.cpp
├── models/                         # ONNX 模型
│   ├── chunk_mesh_v1.onnx          # 区块网格面剔除模型
│   ├── light_baking_v1.onnx        # 光照烘焙模型
│   └── export_models.py            # 模型导出脚本
├── fabric-mod/                     # Fabric 模组（Java 端）
│   ├── build.gradle
│   ├── gradle.properties
│   └── src/main/
│       ├── java/com/vibecoding/carbon/
│       │   ├── CarbonMod.java          # 模组主类
│       │   ├── config/                 # 配置系统
│       │   ├── npu/NPUBridge.java      # JNI 桥接（自动释放原生库）
│       │   ├── command/                # 调试命令
│       │   └── mixin/                  # Mixin 注入点
│       └── resources/
│           ├── native/win-x64/         # 打包在 JAR 内的原生库
│           │   ├── carbon_npu.dll
│           │   ├── onnxruntime.dll
│           │   ├── chunk_mesh_v1.onnx
│           │   └── light_baking_v1.onnx
│           ├── fabric.mod.json
│           └── carbon.mixins.json
├── third_party/                    # 第三方依赖（ORT 头文件/导入库）
├── build.py                        # C++ 编译脚本
├── CMakeLists.txt                  # CMake 配置
└── build/                          # C++ 编译输出
    ├── carbon.lib                  # 静态库
    └── carbon_npu.dll              # 原生 DLL
```

## 架构分层

```
游戏逻辑层（MC 区块 / 实体 / 渲染原始数据）
        ↓
NPU 统一调度层（任务队列 + 优先级调度 + 负载均衡）
        ↓
HAL 硬件抽象接口层（统一 API 屏蔽各厂商硬件差异）
        ↓
可插拔硬件执行后端：ONNX Runtime / OpenVINO / TensorRT / DirectML
        ↓
底层硬件：专用 NPU / 独显 GPU / CPU（兜底）
```

## 核心功能

### P0 - 区块计算加速
- 区块网格（Mesh）顶点批量生成
- 光照烘焙并行预计算

### P1 - 渲染管线减负（规划中）
- SSAO、阴影贴图、景深模糊、体积云预计算
- AI 纹理超分、贴图降噪
- 大气散射前置计算

### P3 - 特色优化（规划中）
- 远景区块后台分块推理
- 多存档热切换 / 模型热重载

## 调度特性

- **优先级任务队列**：区块加载 > 渲染预处理 > 实体/粒子 > 后台更新
- **自动降级**：NPU 占用 > 85% 自动降级至 GPU/CPU
- **熔断机制**：连续失败自动熔断，冷却后恢复
- **多后端负载均衡**：多硬件自动均分计算负载

## 硬件支持

| 硬件 | 后端 | 状态 |
|------|------|------|
| Intel Ultra NPU | OpenVINO | ✅ 已实现 |
| AMD Ryzen AI | DirectML | ✅ 已实现 |
| NVIDIA RTX Tensor Core | TensorRT | ✅ 已实现 |
| 通用 Windows GPU/CPU | ONNX Runtime | ✅ 已实现 |
| Mock（测试用） | MockBackend | ✅ 已实现 |

## ONNX 模型

### chunk_mesh_v1.onnx - 区块网格面剔除

| 项目 | 说明 |
|------|------|
| 输入 | `block_data` [1, 16, 256, 16] int32 - 方块数据 |
| 输出 | `face_mask` [1, 6, 16, 256, 16] float - 6 个面的可见性掩码 |
| 输出 | `vertex_count` [] int32 - 可见面总数 |
| 用途 | 判断区块中哪些面需要渲染，跳过被遮挡的面 |

### light_baking_v1.onnx - 光照烘焙

| 项目 | 说明 |
|------|------|
| 输入 | `sky_light` [1, 16, 256, 16] float - 天空光照 |
| 输入 | `block_light` [1, 16, 256, 16] float - 方块光照 |
| 输入 | `opaque_mask` [1, 16, 256, 16] float - 不透明遮挡掩码 |
| 输出 | `combined_light` [1, 16, 256, 16] float - 最终光照结果 |
| 用途 | 合并天空光和方块光，处理光照传播 |

## 编译说明

### 前置要求

- **Windows 10/11 x64**
- **Visual Studio 2022 BuildTools**（含 MSVC 编译器）
- **Windows 10 SDK**（10.0.26100+）
- **JDK 17+**
- **Python 3.10+**（用于编译脚本和模型导出）

### 第一步：编译 C++ 核心库和 DLL

```bash
python build.py
```

编译产物：
- `build/carbon.lib` - 静态库
- `build/carbon_npu.dll` - 原生 DLL

### 第二步：导出 ONNX 模型

```bash
pip install onnx onnxruntime
python models/export_models.py
```

### 第三步：编译 Fabric 模组

```bash
cd fabric-mod
./gradlew build
```

输出位置：`fabric-mod/build/libs/fabric-mod-0.1.0.jar`

## 配置说明

### 模组配置（config/carbon.json）

```json
{
  "enableNPUAcceleration": true,
  "npuHighUtilizationThreshold": 0.85,
  "maxConsecutiveFailures": 5,
  "circuitBreakerCooldownMs": 5000,
  "enableAutoDegrade": true,
  "enableCircuitBreaker": true,
  "enableChunkMeshAcceleration": true,
  "enableLightBakingAcceleration": true
}
```

### C++ 层配置

```cpp
SchedulerConfig config;
config.enable_npu_acceleration = true;              // 全局总开关
config.enable_auto_degrade = true;                  // 自动降级
config.enable_circuit_breaker = true;               // 熔断机制
config.npu_high_utilization_threshold = 0.85f;      // 降级阈值
config.max_consecutive_failures = 5;                // 最大连续失败次数
config.circuit_breaker_cooldown_ms = 5000;          // 熔断冷却时间
```

## 平台说明

- **正式版**：仅支持 Windows x86_64
- **开发测试**：Linux 下可使用 MockBackend 进行调度逻辑测试
- **模组加载器**：Fabric 1.20.x（后续支持 Forge）

## 许可证

MIT License
