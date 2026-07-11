#pragma once

#include "carbon/hal/npu_backend.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

// TensorRT headers (conditionally included)
#ifdef CARBON_USE_TENSORRT
// #include <NvInfer.h>
// #include <NvOnnxParser.h>
// #include <cuda_runtime.h>
#endif

namespace carbon {

// TensorRT 后端 - NVIDIA Tensor Core 支持
class TensorRTBackend : public NPUBackend {
public:
    TensorRTBackend();
    ~TensorRTBackend() override;

    // NPUBackend 接口实现
    BackendType GetType() const override { return BackendType::TensorRT; }
    std::string GetName() const override { return "TensorRT (NVIDIA Tensor Core)"; }
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
    size_t GetMaxBatchSize() const override { return 16; }
    
    bool HasError() const override { return has_error_.load(); }
    std::string GetLastError() const override { return last_error_; }
    void ResetError() override;

private:
    // 初始化 CUDA
    bool InitializeCUDA();
    
    // 构建 TensorRT 引擎
    bool BuildEngine(const std::string& model_path, const std::string& model_name);
    
    // 加载缓存的引擎
    bool LoadCachedEngine(const std::string& engine_path, const std::string& model_name);
    
    // 保存引擎到缓存
    bool SaveCachedEngine(const std::string& engine_path, const std::string& model_name);
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> has_error_{false};
    std::string last_error_;
    
    // 内存统计
    std::atomic<size_t> total_memory_{0};
    std::atomic<size_t> used_memory_{0};
    
    struct ModelInfo {
        // std::unique_ptr<nvinfer1::ICudaEngine> engine;
        // std::unique_ptr<nvinfer1::IExecutionContext> context;
        
        // 设备端内存指针
        // void* d_input = nullptr;
        // void* d_output = nullptr;
        
        size_t input_size = 0;
        size_t output_size = 0;
        size_t memory_usage = 0;
    };
    
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, ModelInfo> loaded_models_;
    
    // 异步任务ID
    std::atomic<uint64_t> next_async_id_{1};
};

} // namespace carbon
