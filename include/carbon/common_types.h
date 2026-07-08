#pragma once

#include <cstdint>
#include <string>

namespace carbon {

// 任务优先级（从高到低）
enum class TaskPriority : uint8_t {
    Critical = 0,   // 区块加载 - 最高优先级
    High = 1,       // 渲染预处理
    Normal = 2,     // 实体/粒子计算
    Low = 3,        // 后台世界更新
    Background = 4  // 远景区块预加载
};

// 任务类型
enum class TaskType : uint8_t {
    ChunkMeshGen,       // P0: 区块网格顶点生成
    LightBaking,        // P0: 光照烘焙
    SSAO_Preprocess,    // P1: SSAO预处理
    ShadowMapSampling,  // P1: 阴影贴图采样
    DOF_Blur,           // P1: 景深模糊
    VolumetricClouds,   // P1: 体积云预计算
    TextureUpscale,     // P1: AI纹理超分
    AtmosphereScatter,  // P1: 大气散射
    EntityUpdate,       // 实体计算
    ParticleUpdate,     // 粒子计算
    ChunkPreload,       // P3: 远景区块预加载
    Custom              // 自定义任务
};

// 硬件后端类型
enum class BackendType : uint8_t {
    Mock = 0,       // 模拟后端（测试用）
    OpenVINO,       // Intel NPU
    TensorRT,       // NVIDIA Tensor Core
    DirectML,       // DirectML (通用Windows)
    CPU_Fallback    // CPU兜底
};

// 任务状态
enum class TaskStatus : uint8_t {
    Pending,
    Queued,
    Running,
    Completed,
    Failed,
    Cancelled
};

// 任务结果
struct TaskResult {
    bool success = false;
    std::string error_message;
    uint64_t execution_time_us = 0;
    void* output_data = nullptr;
    size_t output_size = 0;
};

} // namespace carbon
