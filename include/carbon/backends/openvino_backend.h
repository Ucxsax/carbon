#pragma once

#include "carbon/hal/npu_backend.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

// OpenVINO headers (conditionally included)
#ifdef CARBON_USE_OPENVINO
#include <openvino/openvino.hpp>
#endif

namespace carbon {

// OpenVINO 后端 - Intel NPU 支持
class OpenVINOBackend : public NPUBackend {
public:
    OpenVINOBackend();
    ~OpenVINOBackend() override;

    // NPUBackend 接口实现
    BackendType GetType() const override { return BackendType::OpenVINO; }
    std::string GetName() const override { return "OpenVINO (Intel NPU)"; }
    bool IsAvailable() const override;
    
    bool Initialize() override;
    void Shutdown() override;
    
    float GetUtilization() const override;
    size_t GetTotalMemory() const override;
    size_t GetUsedMemory() const override;
    
    bool LoadModel(const std::string& model_path, const std::string& model_name) override;
    bool UnloadModel(const std::string& model_name) override;
    bool IsModelLoaded(const std::string& model_name) const override;
    
    TaskResult RunInference(
        const std::string& model_name,
        const std::vector<TensorDesc>& inputs,
        std::vector<TensorDesc>& outputs
    ) override;
    
    uint64_t RunInferenceAsync(
        const std::string& model_name,
        const std::vector<TensorDesc>& inputs,
        TaskCompleteCallback callback
    ) override;
    
    bool SupportsBatchProcessing() const override { return true; }
    size_t GetMaxBatchSize() const override { return 8; }
    
    bool HasError() const override { return has_error_.load(); }
    std::string GetLastError() const override { return last_error_; }
    void ResetError() override;

private:
    // 初始化 OpenVINO 运行时
    bool InitializeOpenVINO();
    
    // 检测 Intel NPU 设备
    bool DetectIntelNPU();
    
    // 编译模型
    bool CompileModel(const std::string& model_path, const std::string& model_name);
    
    // 转换 TensorDesc 到 OpenVINO tensor
    // #ifdef CARBON_USE_OPENVINO
    // ov::Tensor ConvertToOVTensor(const TensorDesc& desc);
    // #endif
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> has_error_{false};
    std::string last_error_;
    
    // 内存统计
    std::atomic<size_t> total_memory_{0};
    std::atomic<size_t> used_memory_{0};
    
    // 模型管理
    struct ModelInfo {
        // #ifdef CARBON_USE_OPENVINO
        // std::shared_ptr<ov::Model> model;
        // std::shared_ptr<ov::CompiledModel> compiled_model;
        // std::unique_ptr<ov::InferRequest> infer_request;
        // #endif
        size_t memory_usage = 0;
    };
    
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, ModelInfo> loaded_models_;
    
#ifdef CARBON_USE_OPENVINO
    std::unique_ptr<ov::Core> ov_core_;
    ov::CompiledModel current_model_;
    ov::InferRequest infer_request_;
#endif
    
    // 异步任务ID
    std::atomic<uint64_t> next_async_id_{1};
};

} // namespace carbon
