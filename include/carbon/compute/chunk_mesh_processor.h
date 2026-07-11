#pragma once

#include "carbon/common_types.h"
#include "carbon/hal/npu_backend.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace carbon {

// 区块尺寸常量
constexpr int CHUNK_SIZE_X = 16;
constexpr int CHUNK_SIZE_Y = 256;
constexpr int CHUNK_SIZE_Z = 16;
constexpr int CHUNK_VOLUME = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

// 顶点数据结构
struct Vertex {
    float x, y, z;      // 位置
    float nx, ny, nz;    // 法线
    float u, v;          // UV 坐标
    uint32_t color;      // 颜色 (ARGB)
};

// 区块网格生成结果
struct ChunkMeshResult {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    size_t face_count = 0;
    size_t visible_face_count = 0;
};

// 区块网格 NPU 推理处理器
class ChunkMeshProcessor {
public:
    ChunkMeshProcessor();
    ~ChunkMeshProcessor() = default;
    
    // 初始化处理器
    bool Initialize(std::shared_ptr<NPUBackend> backend);
    
    // 处理区块网格生成任务
    // 输入: 16x256x16 方块ID数组
    // 输出: 顶点缓冲区
    TaskResult ProcessChunkMesh(
        int chunk_x, int chunk_z,
        const int* block_data,
        size_t data_size,
        ChunkMeshResult& result
    );
    
    // 异步处理
    uint64_t ProcessChunkMeshAsync(
        int chunk_x, int chunk_z,
        const int* block_data,
        size_t data_size,
        std::function<void(uint64_t, const TaskResult&, const ChunkMeshResult&)> callback
    );
    
    // 预加载模型
    bool LoadModel(const std::string& model_path);
    
private:
    // 准备输入张量
    std::vector<TensorDesc> PrepareInputs(
        int chunk_x, int chunk_z,
        const int* block_data,
        size_t data_size
    );
    
    // 解析输出张量
    void ParseOutputs(
        const std::vector<TensorDesc>& outputs,
        ChunkMeshResult& result
    );
    
    // CPU 兜底计算
    void CPUFallback(
        const int* block_data,
        ChunkMeshResult& result
    );
    
    // 检查方块是否可见（面剔除）
    bool IsBlockVisible(const int* block_data, int x, int y, int z);
    
    // 获取方块颜色
    uint32_t GetBlockColor(int block_id);
    
    std::shared_ptr<NPUBackend> backend_;
    bool initialized_ = false;
    
    // 模型名称
    static constexpr const char* MODEL_NAME = "chunk_mesh_v1";
};

} // namespace carbon
