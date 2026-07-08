#pragma once

#include "carbon/hal/npu_backend.h"
#include <unordered_map>
#include <mutex>
#include <atomic>

namespace carbon {

// 模拟后端 - 用于测试和无硬件环境
class MockBackend : public NPUBackend {
public:
    MockBackend();
    ~MockBackend() override;
    
    // 后端信息
    BackendType GetType() const override { return BackendType::Mock; }
    std::string GetName() const override { return "MockBackend"; }
    bool IsAvailable() const override { return true; }
    
    // 生命周期
    bool Initialize() override;
    void Shutdown() override;
    
    // 硬件状态（模拟）
    float GetUtilization() const override;
    size_t GetTotalMemory() const override { return 2ULL * 1024 * 1024 * 1024; } // 2GB
    size_t GetUsedMemory() const override;
    
    // 模型管理
    bool LoadModel(const std::string& model_path, const std::string& model_name) override;
    bool UnloadModel(const std::string& model_name) override;
    bool IsModelLoaded(const std::string& model_name) const override;
    
    // 同步推理
    TaskResult RunInference(
        const std::string& model_name,
        const std::vector<TensorDesc>& inputs,
        std::vector<TensorDesc>& outputs
    ) override;
    
    // 异步推理
    uint64_t RunInferenceAsync(
        const std::string& model_name,
        const std::vector<TensorDesc>& inputs,
        TaskCompleteCallback callback
    ) override;
    
    // 批量支持
    bool SupportsBatchProcessing() const override { return true; }
    size_t GetMaxBatchSize() const override { return 32; }
    
    // 测试控制接口
    void SetSimulatedUtilization(float util) { simulated_util_ = util; }
    void SetSimulatedDelayMs(uint32_t ms) { simulated_delay_ms_ = ms; }
    void SetFailureRate(float rate) { failure_rate_ = rate; } // 0.0 ~ 1.0
    
private:
    std::atomic<bool> initialized_{false};
    std::atomic<float> simulated_util_{0.3f};
    std::atomic<uint32_t> simulated_delay_ms_{10};
    std::atomic<float> failure_rate_{0.0f};
    std::atomic<size_t> used_memory_{0};
    
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, bool> loaded_models_;
    
    std::atomic<uint64_t> next_async_id_{1};
    std::string last_error_;
};

} // namespace carbon
