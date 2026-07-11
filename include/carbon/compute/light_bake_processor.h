#pragma once

#include "carbon/common_types.h"
#include "carbon/hal/npu_backend.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace carbon {

// 光照烘焙结果
struct LightBakeResult {
    std::vector<uint8_t> sky_light;      // 天空光 16x256x16
    std::vector<uint8_t> block_light;    // 方块光 16x256x16
    std::vector<uint8_t> combined_light; // 合并光照 16x256x16
    bool success = false;
};

// 光照烘焙 NPU 推理处理器
class LightBakeProcessor {
public:
    LightBakeProcessor();
    ~LightBakeProcessor() = default;
    
    // 初始化处理器
    bool Initialize(std::shared_ptr<NPUBackend> backend);
    
    // 处理光照烘焙任务
    // 输入: 天空光 + 方块光
    // 输出: 优化后的光照数据
    TaskResult ProcessLightBaking(
        int chunk_x, int chunk_z,
        const uint8_t* sky_light,
        const uint8_t* block_light,
        size_t data_size,
        LightBakeResult& result
    );
    
    // 异步处理
    uint64_t ProcessLightBakingAsync(
        int chunk_x, int chunk_z,
        const uint8_t* sky_light,
        const uint8_t* block_light,
        size_t data_size,
        std::function<void(uint64_t, const TaskResult&, const LightBakeResult&)> callback
    );
    
    // 预加载模型
    bool LoadModel(const std::string& model_path);
    
private:
    // 准备输入张量
    std::vector<TensorDesc> PrepareInputs(
        int chunk_x, int chunk_z,
        const uint8_t* sky_light,
        const uint8_t* block_light,
        size_t data_size
    );
    
    // 解析输出张量
    void ParseOutputs(
        const std::vector<TensorDesc>& outputs,
        LightBakeResult& result
    );
    
    // CPU 兜底计算
    void CPUFallback(
        const uint8_t* sky_light,
        const uint8_t* block_light,
        LightBakeResult& result
    );
    
    // 光照传播算法（CPU版本）
    void PropagateLight(
        uint8_t* light_map,
        int start_x, int start_y, int start_z,
        int intensity
    );
    
    // 合并天空光和方块光
    void CombineLight(
        const uint8_t* sky_light,
        const uint8_t* block_light,
        uint8_t* combined
    );
    
    std::shared_ptr<NPUBackend> backend_;
    bool initialized_ = false;
    
    // 模型名称
    static constexpr const char* MODEL_NAME = "light_baking_v1";
    
    // 光照衰减表
    static constexpr int LIGHT_ATTENUATION[16] = {
        15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
    };
};

} // namespace carbon
