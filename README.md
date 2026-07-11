# Carbon - Minecraft NPU 加速模块

PC 端 Minecraft NPU 异构算力加速模组。通过调用 PC 端硬件 NPU 分流、分担 Minecraft 原有 CPU、GPU 计算负载。

## 项目结构

```
carbon/
├── CMakeLists.txt              # C++ 核心库 CMake 配置
├── include/                    # C++ 头文件
│   └── carbon/
│       ├── common_types.h      # 公共类型定义
│       ├── scheduler/          # 任务调度层
│       ├── hal/                # 硬件抽象层
│       ├── backends/           # 硬件后端
│       └── utils/              # 工具类
├── src/                        # C++ 源码
├── tests/                      # C++ 单元测试
├── jni/                        # JNI 桥接层（Java <-> C++）
│   └── carbon_npu_jni.cpp
├── fabric-mod/                 # Fabric 模组（Java 端）
│   ├── build.gradle
│   ├── gradle.properties
│   ├── settings.gradle
│   └── src/main/
│       ├── java/com/vibecoding/carbon/
│       │   ├── CarbonMod.java          # 模组主类
│       │   ├── config/                 # 配置系统
│       │   ├── npu/NPUBridge.java      # JNI 桥接 Java 端
│       │   └── mixin/                  # Mixin 注入点
│       └── resources/
│           ├── fabric.mod.json
│           └── carbon.mixins.json
└── build/                      # C++ 编译输出
```

## 架构分层

```
游戏逻辑层（MC 区块 / 实体 / 渲染原始数据）
        ↓
NPU 统一调度层（任务队列 + 优先级调度 + 负载均衡）
        ↓
HAL 硬件抽象接口层（统一 API 屏蔽各厂商硬件差异）
        ↓
可插拔硬件执行后端：OpenVINO / TensorRT / AMD Lemonade / QNN
        ↓
底层硬件：专用 NPU / 独显 GPU / CPU（兜底）
```

## 核心功能

### P0 - 区块计算加速
- 区块网格（Mesh）顶点批量生成
- 光照烘焙并行预计算

### P1 - 渲染管线减负
- SSAO、阴影贴图、景深模糊、体积云预计算
- AI 纹理超分、贴图降噪
- 大气散射前置计算

### P3 - 特色优化
- 远景区块后台分块推理
- 多存档热切换 / 模型热重载

## 调度特性

- **优先级任务队列**：区块加载 > 渲染预处理 > 实体/粒子 > 后台更新
- **自动降级**：NPU 占用 > 85% 自动降级至 GPU/CPU
- **熔断机制**：连续失败自动熔断，冷却后恢复
- **多后端负载均衡**：多硬件自动均分计算负载

## 支持硬件

| 硬件 | 后端 | 状态 |
|------|------|------|
| Intel Ultra NPU | OpenVINO | 规划中 |
| AMD Ryzen AI | DirectML | 规划中 |
| NVIDIA RTX Tensor Core | TensorRT | 规划中 |
| 通用 Windows | DirectML | 规划中 |
| Mock (测试用) | MockBackend | ✅ 已实现 |

## 编译说明

### 前置要求
- **Java 17+**（Minecraft 1.20 要求）
- **Gradle 8+**（或使用 gradle wrapper）
- **Windows 11 22H2+**（运行时，开发可以是其他系统）
- **CMake 3.20+**（编译 C++ 原生库）
- **Visual Studio 2022**（Windows 下编译原生 DLL）

### 第一步：编译 C++ 核心库

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### 第二步：编译 Fabric 模组 Jar

```bash
cd fabric-mod
./gradlew build
```

输出位置：`fabric-mod/build/libs/carbon-0.1.0.jar`

### 运行单元测试

```bash
cd build
./tests/carbon_tests
```

## 平台说明

- **正式版**：仅支持 Windows 11 22H2+ x86_64
- **开发测试**：Linux 下可使用 MockBackend 进行调度逻辑测试
- **模组加载器**：Fabric（1.20.x），后续支持 Forge

## 开关配置

### C++ 层
```cpp
SchedulerConfig config;
config.enable_npu_acceleration = true;   // 全局总开关
config.enable_auto_degrade = true;       // 自动降级
config.enable_circuit_breaker = true;    // 熔断机制
config.npu_high_utilization_threshold = 0.85f; // 降级阈值
```

### 模组配置（config/carbon.json）
```json
{
  "enableNPUAcceleration": true,
  "npuHighUtilizationThreshold": 0.85,
  "maxConsecutiveFailures": 5,
  "enableChunkMeshAcceleration": true,
  "enableLightBakingAcceleration": true
}
```

## 当前实现状态

### ✅ 已完成
- C++ 核心调度层（任务队列、优先级调度、熔断机制、负载降级）
- Mock 硬件后端（测试用）
- Fabric 模组基础框架（主类、配置、Mixin 注入点）
- JNI 桥接层骨架（Java <-> C++ 接口定义）
- 17 个 C++ 单元测试全部通过

### 🚧 待实现
- 真实硬件后端（OpenVINO / TensorRT / DirectML）
- 区块网格生成的具体 NPU 计算逻辑
- 光照烘焙的具体 NPU 计算逻辑
- JNI 异步回调机制
- Mod Menu / Cloth Config 配置界面
- Sodium / Iris 兼容适配
