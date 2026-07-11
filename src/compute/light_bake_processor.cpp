#include "carbon/compute/light_bake_processor.h"
#include "carbon/compute/chunk_mesh_processor.h"
#include "carbon/utils/logger.h"
#include <cstring>
#include <algorithm>
#include <queue>

namespace carbon {

LightBakeProcessor::LightBakeProcessor() = default;

bool LightBakeProcessor::Initialize(std::shared_ptr<NPUBackend> backend) {
    if (!backend) {
        CARBON_LOG_ERROR("LightBakeProcessor: Backend is null");
        return false;
    }
    
    backend_ = backend;
    initialized_ = true;
    
    CARBON_LOG_INFO("LightBakeProcessor initialized with backend: " + backend_->GetName());
    return true;
}

TaskResult LightBakeProcessor::ProcessLightBaking(
    int chunk_x, int chunk_z,
    const uint8_t* sky_light,
    const uint8_t* block_light,
    size_t data_size,
    LightBakeResult& result
) {
    TaskResult task_result;
    
    if (!initialized_) {
        task_result.success = false;
        task_result.error_message = "Processor not initialized";
        return task_result;
    }
    
    if (!sky_light || !block_light || data_size < CHUNK_VOLUME) {
        task_result.success = false;
        task_result.error_message = "Invalid input data";
        return task_result;
    }
    
    // 准备输入
    auto inputs = PrepareInputs(chunk_x, chunk_z, sky_light, block_light, data_size);
    
    // 输出张量
    std::vector<TensorDesc> outputs;
    
    // 尝试 NPU 推理
    task_result = backend_->RunInference(MODEL_NAME, inputs, outputs);
    
    if (task_result.success) {
        // 解析输出
        ParseOutputs(outputs, result);
        result.success = true;
        CARBON_LOG_DEBUG("Light baking NPU inference completed");
    } else {
        // NPU 推理失败，使用 CPU 兜底
        CARBON_LOG_WARN("NPU inference failed, using CPU fallback: " + task_result.error_message);
        CPUFallback(sky_light, block_light, result);
        
        task_result.success = true;
        task_result.error_message = "";
    }
    
    return task_result;
}

uint64_t LightBakeProcessor::ProcessLightBakingAsync(
    int chunk_x, int chunk_z,
    const uint8_t* sky_light,
    const uint8_t* block_light,
    size_t data_size,
    std::function<void(uint64_t, const TaskResult&, const LightBakeResult&)> callback
) {
    // 复制输入数据
    std::vector<uint8_t> sky_copy(sky_light, sky_light + data_size);
    std::vector<uint8_t> block_copy(block_light, block_light + data_size);
    
    return backend_->RunInferenceAsync(
        MODEL_NAME,
        {},
        [this, chunk_x, chunk_z, sky_copy, block_copy, callback](uint64_t task_id, const TaskResult& result) {
            LightBakeResult bake_result;
            if (result.success) {
                CPUFallback(sky_copy.data(), block_copy.data(), bake_result);
                bake_result.success = true;
            }
            callback(task_id, result, bake_result);
        }
    );
}

bool LightBakeProcessor::LoadModel(const std::string& model_path) {
    if (!backend_) return false;
    
    return backend_->LoadModel(model_path, MODEL_NAME);
}

// ========== 私有方法 ==========

std::vector<TensorDesc> LightBakeProcessor::PrepareInputs(
    int chunk_x, int chunk_z,
    const uint8_t* sky_light,
    const uint8_t* block_light,
    size_t data_size
) {
    std::vector<TensorDesc> inputs;
    
    // 输入1: 天空光数据 [1, 16, 256, 16]
    TensorDesc sky_input;
    sky_input.data_type = TensorDesc::DataType::UInt8;
    sky_input.shape = {1, CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z};
    sky_input.size_bytes = data_size;
    sky_input.data = new uint8_t[data_size];
    std::memcpy(sky_input.data, sky_light, data_size);
    inputs.push_back(sky_input);
    
    // 输入2: 方块光数据 [1, 16, 256, 16]
    TensorDesc block_input;
    block_input.data_type = TensorDesc::DataType::UInt8;
    block_input.shape = {1, CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z};
    block_input.size_bytes = data_size;
    block_input.data = new uint8_t[data_size];
    std::memcpy(block_input.data, block_light, data_size);
    inputs.push_back(block_input);
    
    // 输入3: 区块坐标
    TensorDesc coord_input;
    coord_input.data_type = TensorDesc::DataType::Int32;
    coord_input.shape = {2};
    coord_input.size_bytes = 2 * sizeof(int);
    int coords[2] = {chunk_x, chunk_z};
    coord_input.data = new int[2];
    std::memcpy(coord_input.data, coords, coord_input.size_bytes);
    inputs.push_back(coord_input);
    
    return inputs;
}

void LightBakeProcessor::ParseOutputs(
    const std::vector<TensorDesc>& outputs,
    LightBakeResult& result
) {
    if (outputs.size() < 2) {
        return;
    }
    
    // 解析天空光输出
    const auto& sky_output = outputs[0];
    if (sky_output.data && sky_output.size_bytes >= CHUNK_VOLUME) {
        result.sky_light.resize(CHUNK_VOLUME);
        std::memcpy(result.sky_light.data(), sky_output.data, CHUNK_VOLUME);
    }
    
    // 解析方块光输出
    const auto& block_output = outputs[1];
    if (block_output.data && block_output.size_bytes >= CHUNK_VOLUME) {
        result.block_light.resize(CHUNK_VOLUME);
        std::memcpy(result.block_light.data(), block_output.data, CHUNK_VOLUME);
    }
    
    // 合并光照
    result.combined_light.resize(CHUNK_VOLUME);
    CombineLight(result.sky_light.data(), result.block_light.data(), result.combined_light.data());
}

void LightBakeProcessor::CPUFallback(
    const uint8_t* sky_light,
    const uint8_t* block_light,
    LightBakeResult& result
) {
    // CPU 兜底：简化光照传播
    result.sky_light.resize(CHUNK_VOLUME);
    result.block_light.resize(CHUNK_VOLUME);
    result.combined_light.resize(CHUNK_VOLUME);
    
    // 复制输入
    std::memcpy(result.sky_light.data(), sky_light, CHUNK_VOLUME);
    std::memcpy(result.block_light.data(), block_light, CHUNK_VOLUME);
    
    // 对天空光进行简单传播
    // 从顶部向下衰减
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            int max_light = 15;
            for (int y = CHUNK_SIZE_Y - 1; y >= 0; y--) {
                int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
                
                if (result.sky_light[index] > 0) {
                    max_light = result.sky_light[index];
                } else if (max_light > 0) {
                    result.sky_light[index] = max_light;
                    max_light = std::max(0, max_light - 1);
                }
            }
        }
    }
    
    // 对方块光进行简单扩散
    // 这里简化处理，实际需要 BFS/DFS 传播
    
    // 合并光照
    CombineLight(result.sky_light.data(), result.block_light.data(), result.combined_light.data());
    
    CARBON_LOG_DEBUG("CPU fallback light baking completed");
}

void LightBakeProcessor::PropagateLight(
    uint8_t* light_map,
    int start_x, int start_y, int start_z,
    int intensity
) {
    // BFS 光照传播
    std::queue<std::tuple<int, int, int, int>> bfs_queue;
    bfs_queue.push({start_x, start_y, start_z, intensity});
    
    while (!bfs_queue.empty()) {
        auto [x, y, z, light] = bfs_queue.front();
        bfs_queue.pop();
        
        if (light <= 0) continue;
        
        int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
        if (x < 0 || x >= CHUNK_SIZE_X ||
            y < 0 || y >= CHUNK_SIZE_Y ||
            z < 0 || z >= CHUNK_SIZE_Z) {
            continue;
        }
        
        if (light_map[index] >= light) continue;
        
        light_map[index] = light;
        
        // 传播到相邻方块
        int next_light = light - 1;
        if (next_light > 0) {
            bfs_queue.push({x + 1, y, z, next_light});
            bfs_queue.push({x - 1, y, z, next_light});
            bfs_queue.push({x, y + 1, z, next_light});
            bfs_queue.push({x, y - 1, z, next_light});
            bfs_queue.push({x, y, z + 1, next_light});
            bfs_queue.push({x, y, z - 1, next_light});
        }
    }
}

void LightBakeProcessor::CombineLight(
    const uint8_t* sky_light,
    const uint8_t* block_light,
    uint8_t* combined
) {
    // Minecraft 光照合并规则
    // max(sky_light, block_light)
    for (size_t i = 0; i < CHUNK_VOLUME; i++) {
        combined[i] = std::max(sky_light[i], block_light[i]);
    }
}

} // namespace carbon
