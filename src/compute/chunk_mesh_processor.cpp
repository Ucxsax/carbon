#include "carbon/compute/chunk_mesh_processor.h"
#include "carbon/utils/logger.h"
#include <cstring>
#include <algorithm>

namespace carbon {

ChunkMeshProcessor::ChunkMeshProcessor() = default;

bool ChunkMeshProcessor::Initialize(std::shared_ptr<NPUBackend> backend) {
    if (!backend) {
        CARBON_LOG_ERROR("ChunkMeshProcessor: Backend is null");
        return false;
    }
    
    backend_ = backend;
    initialized_ = true;
    
    CARBON_LOG_INFO("ChunkMeshProcessor initialized with backend: " + backend_->GetName());
    return true;
}

TaskResult ChunkMeshProcessor::ProcessChunkMesh(
    int chunk_x, int chunk_z,
    const int* block_data,
    size_t data_size,
    ChunkMeshResult& result
) {
    TaskResult task_result;
    
    if (!initialized_) {
        task_result.success = false;
        task_result.error_message = "Processor not initialized";
        return task_result;
    }
    
    if (!block_data || data_size < CHUNK_VOLUME) {
        task_result.success = false;
        task_result.error_message = "Invalid input data";
        return task_result;
    }
    
    // 准备输入
    auto inputs = PrepareInputs(chunk_x, chunk_z, block_data, data_size);
    
    // 输出张量（预留空间）
    std::vector<TensorDesc> outputs;
    
    // 尝试 NPU 推理
    task_result = backend_->RunInference(MODEL_NAME, inputs, outputs);
    
    if (task_result.success) {
        // 解析输出
        ParseOutputs(outputs, result);
        CARBON_LOG_DEBUG("Chunk mesh NPU inference completed: " + 
                        std::to_string(result.vertices.size()) + " vertices");
    } else {
        // NPU 推理失败，使用 CPU 兜底
        CARBON_LOG_WARN("NPU inference failed, using CPU fallback: " + task_result.error_message);
        CPUFallback(block_data, result);
        
        task_result.success = true;
        task_result.error_message = "";
    }
    
    return task_result;
}

uint64_t ChunkMeshProcessor::ProcessChunkMeshAsync(
    int chunk_x, int chunk_z,
    const int* block_data,
    size_t data_size,
    std::function<void(uint64_t, const TaskResult&, const ChunkMeshResult&)> callback
) {
    // 复制输入数据
    std::vector<int> block_data_copy(block_data, block_data + data_size);
    
    return backend_->RunInferenceAsync(
        MODEL_NAME,
        {},
        [this, chunk_x, chunk_z, block_data_copy, callback](uint64_t task_id, const TaskResult& result) {
            ChunkMeshResult mesh_result;
            if (result.success) {
                // 注意：这里简化处理，实际需要从异步推理获取输出
                CPUFallback(block_data_copy.data(), mesh_result);
            }
            callback(task_id, result, mesh_result);
        }
    );
}

bool ChunkMeshProcessor::LoadModel(const std::string& model_path) {
    if (!backend_) return false;
    
    return backend_->LoadModel(model_path, MODEL_NAME);
}

// ========== 私有方法 ==========

std::vector<TensorDesc> ChunkMeshProcessor::PrepareInputs(
    int chunk_x, int chunk_z,
    const int* block_data,
    size_t data_size
) {
    std::vector<TensorDesc> inputs;
    
    // 输入1: 方块ID数据 [1, 16, 256, 16] (NCHW格式)
    TensorDesc block_input;
    block_input.data_type = TensorDesc::DataType::Int32;
    block_input.shape = {1, CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z};
    
    // 分配内存并复制数据
    block_input.size_bytes = data_size * sizeof(int);
    block_input.data = new int[data_size];
    std::memcpy(block_input.data, block_data, block_input.size_bytes);
    
    inputs.push_back(block_input);
    
    // 输入2: 区块坐标
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

void ChunkMeshProcessor::ParseOutputs(
    const std::vector<TensorDesc>& outputs,
    ChunkMeshResult& result
) {
    // 解析 NPU 输出的顶点数据
    // 输出格式: [vertex_count, 10] (x,y,z, nx,ny,nz, u,v, color_r, color_g)
    
    if (outputs.empty()) {
        return;
    }
    
    const auto& output = outputs[0];
    
    if (output.data && output.size_bytes > 0) {
        // 解析顶点数据
        size_t vertex_count = output.size_bytes / (10 * sizeof(float));
        const float* data = static_cast<const float*>(output.data);
        
        result.vertices.reserve(vertex_count);
        
        for (size_t i = 0; i < vertex_count; i++) {
            Vertex v;
            size_t offset = i * 10;
            
            v.x = data[offset + 0];
            v.y = data[offset + 1];
            v.z = data[offset + 2];
            
            v.nx = data[offset + 3];
            v.ny = data[offset + 4];
            v.nz = data[offset + 5];
            
            v.u = data[offset + 6];
            v.v = data[offset + 7];
            
            // 将 RGB 转换为 ARGB
            uint8_t r = static_cast<uint8_t>(data[offset + 8] * 255.0f);
            uint8_t g = static_cast<uint8_t>(data[offset + 9] * 255.0f);
            uint8_t b = static_cast<uint8_t>((data[offset + 8] + data[offset + 9]) * 127.5f);
            v.color = (0xFF << 24) | (r << 16) | (g << 8) | b;
            
            result.vertices.push_back(v);
        }
        
        // 生成索引（简单三角形列表）
        result.indices.reserve(vertex_count);
        for (uint32_t i = 0; i < vertex_count; i++) {
            result.indices.push_back(i);
        }
        
        result.face_count = vertex_count / 3;
        result.visible_face_count = result.face_count;
    }
}

void ChunkMeshProcessor::CPUFallback(
    const int* block_data,
    ChunkMeshResult& result
) {
    // CPU 兜底：简单的面剔除和顶点生成
    result.vertices.clear();
    result.indices.clear();
    result.face_count = 0;
    result.visible_face_count = 0;
    
    // 遍历所有方块
    for (int y = 0; y < CHUNK_SIZE_Y; y++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
                int block_id = block_data[index];
                
                if (block_id == 0) continue; // 空气
                
                // 检查6个面是否可见
                // 这里简化处理，实际需要检查相邻方块
                
                // 生成简单立方体顶点
                uint32_t color = GetBlockColor(block_id);
                
                // 每个面4个顶点，6个面 = 24个顶点
                // 简化版本：只生成前面
                float bx = static_cast<float>(x);
                float by = static_cast<float>(y);
                float bz = static_cast<float>(z);
                
                // 前面 (z+)
                Vertex v0 = {bx, by, bz + 1, 0, 0, 1, 0, 0, color};
                Vertex v1 = {bx + 1, by, bz + 1, 0, 0, 1, 1, 0, color};
                Vertex v2 = {bx + 1, by + 1, bz + 1, 0, 0, 1, 1, 1, color};
                Vertex v3 = {bx, by + 1, bz + 1, 0, 0, 1, 0, 1, color};
                
                uint32_t base_idx = static_cast<uint32_t>(result.vertices.size());
                
                result.vertices.push_back(v0);
                result.vertices.push_back(v1);
                result.vertices.push_back(v2);
                result.vertices.push_back(v3);
                
                result.indices.push_back(base_idx);
                result.indices.push_back(base_idx + 1);
                result.indices.push_back(base_idx + 2);
                result.indices.push_back(base_idx);
                result.indices.push_back(base_idx + 2);
                result.indices.push_back(base_idx + 3);
                
                result.face_count++;
                result.visible_face_count++;
            }
        }
    }
    
    CARBON_LOG_DEBUG("CPU fallback generated " + std::to_string(result.vertices.size()) + " vertices");
}

bool ChunkMeshProcessor::IsBlockVisible(const int* block_data, int x, int y, int z) {
    // 检查方块是否在区块内
    if (x < 0 || x >= CHUNK_SIZE_X ||
        y < 0 || y >= CHUNK_SIZE_Y ||
        z < 0 || z >= CHUNK_SIZE_Z) {
        return true; // 边界外视为可见
    }
    
    int index = (y * CHUNK_SIZE_Z + z) * CHUNK_SIZE_X + x;
    return block_data[index] == 0; // 相邻是空气则可见
}

uint32_t ChunkMeshProcessor::GetBlockColor(int block_id) {
    // 根据方块ID返回颜色
    // 这里简化处理，实际需要从方块注册表获取
    switch (block_id % 16) {
        case 0: return 0xFF808080; // 石头
        case 1: return 0xFF4A7023; // 草方块
        case 2: return 0xFF8B4513; // 泥土
        case 3: return 0xFFD2B48C; // 沙子
        case 4: return 0xFF2F4F4F; // 圆石
        case 5: return 0xFFDAA520; // 木板
        case 6: return 0xFF228B22; // 树叶
        case 7: return 0xFF8B0000; // 红石
        case 8: return 0xFF4169E1; // 水
        case 9: return 0xFFFFFF00; // 岩浆
        case 10: return 0xFFFFFFFF; // 玻璃
        case 11: return 0xFFC0C0C0; // 铁块
        case 12: return 0xFFFFD700; // 金块
        case 13: return 0xFFE5E5E5; // 钻石块
        case 14: return 0xFF000000; // 黑曜石
        case 15: return 0xFF90EE90; // 苔石
        default: return 0xFF808080;
    }
}

} // namespace carbon
