#pragma once

#include "carbon/common_types.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace carbon {

// 计算张量描述
struct TensorDesc {
    std::vector<int64_t> shape;
    enum class DataType {
        Float32,
        Float16,
        Int8,
        Int32,
        UInt8
    } data_type = DataType::Float32;
    void* data = nullptr;
    size_t size_bytes = 0;
};

// NPU后端回调
using TaskCompleteCallback = std::function<void(uint64_t task_id, const TaskResult& result)>;

// HAL 硬件抽象层基类
class NPUBackend {
public:
    virtual ~NPUBackend() = default;

    // 后端信息
    virtual BackendType GetType() const = 0;
    virtual std::string GetName() const = 0;
    virtual bool IsAvailable() const = 0;
    
    // 生命周期
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    
    // 硬件状态
    virtual float GetUtilization() const = 0;  // 0.0 ~ 1.0
    virtual size_t GetTotalMemory() const = 0;  // bytes
    virtual size_t GetUsedMemory() const = 0;   // bytes
    
    // 模型编译/加载
    virtual bool LoadModel(const std::string& model_path, const std::string& model_name) = 0;
    virtual bool UnloadModel(const std::string& model_name) = 0;
    virtual bool IsModelLoaded(const std::string& model_name) const = 0;
    
    // 同步推理
    virtual TaskResult RunInference(
        const std::string& model_name,
        const std::vector<TensorDesc>& inputs,
        std::vector<TensorDesc>& outputs
    ) = 0;
    
    // 异步推理
    virtual uint64_t RunInferenceAsync(
        const std::string& model_name,
        const std::vector<TensorDesc>& inputs,
        TaskCompleteCallback callback
    ) = 0;
    
    // 批量任务支持
    virtual bool SupportsBatchProcessing() const { return false; }
    virtual size_t GetMaxBatchSize() const { return 1; }
    
    // 熔断检测
    virtual bool HasError() const { return false; }
    virtual std::string GetLastError() const { return ""; }
    virtual void ResetError() {}
};

// 后端工厂
class BackendFactory {
public:
    static std::unique_ptr<NPUBackend> CreateBackend(BackendType type);
    static std::vector<BackendType> GetAvailableBackends();
};

} // namespace carbon
