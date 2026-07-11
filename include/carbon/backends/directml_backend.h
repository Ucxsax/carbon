#pragma once

#include "carbon/hal/npu_backend.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

// Windows DirectML headers
#ifdef CARBON_PLATFORM_WINDOWS
#include <d3d12.h>
#include <dxgi1_6.h>
// #include <wrl/client.h>
// #include <DirectML.h>
#endif

namespace carbon {

// DirectML 后端 - AMD NPU / Generic Windows GPU 支持
// 使用 ONNX Runtime + DirectML 执行提供器
class DirectMLBackend : public NPUBackend {
public:
    DirectMLBackend();
    ~DirectMLBackend() override;

    // NPUBackend 接口实现
    BackendType GetType() const override { return BackendType::DirectML; }
    std::string GetName() const override { return "DirectML (AMD/Generic Windows)"; }
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
    size_t GetMaxBatchSize() const override { return 4; }
    
    bool HasError() const override { return has_error_.load(); }
    std::string GetLastError() const override { return last_error_; }
    void ResetError() override;

private:
    // 初始化 D3D12 设备
    bool InitializeD3D12();
    
    // 初始化 DirectML
    bool InitializeDirectML();
    
    // 创建 ONNX Runtime Session (使用 DirectML EP)
    bool CreateOrtSession(const std::string& model_path, const std::string& model_name);
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> has_error_{false};
    std::string last_error_;
    
    // 内存统计
    std::atomic<size_t> total_memory_{0};
    std::atomic<size_t> used_memory_{0};
    
#ifdef CARBON_PLATFORM_WINDOWS
    // D3D12 设备
    // Microsoft::WRL::ComPtr<ID3D12Device> d3d12_device_;
    // Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter_;
    // Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
    
    // DirectML
    // Microsoft::WRL::ComPtr<IDMLDevice> dml_device_;
    // Microsoft::WRL::ComPtr<IDMLCommandRecorder> dml_recorder_;
#endif
    
    // ONNX Runtime (使用 DirectML EP)
    // 需要 onnxruntime_dml.dll
    // std::unique_ptr<Ort::Env> ort_env_;
    
    struct ModelInfo {
        // Ort::Session* session = nullptr;
        size_t memory_usage = 0;
    };
    
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, ModelInfo> loaded_models_;
    
    // 异步任务ID
    std::atomic<uint64_t> next_async_id_{1};
};

} // namespace carbon
