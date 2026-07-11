#pragma once
// OrtBackend - ONNX Runtime via LoadLibrary (no ORT headers needed at compile time)

#ifdef CARBON_PLATFORM_WINDOWS

#include "carbon/hal/npu_backend.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <windows.h>

namespace carbon {

class OrtBackend : public NPUBackend {
public:
    OrtBackend();
    ~OrtBackend() override;

    BackendType GetType() const override { return BackendType::CPU_Fallback; }
    std::string GetName() const override { return "ONNX Runtime (CPU)"; }
    bool IsAvailable() const override;
    bool Initialize() override;
    void Shutdown() override;
    float GetUtilization() const override;
    size_t GetTotalMemory() const override;
    size_t GetUsedMemory() const override;
    bool LoadModel(const std::string& model_path, const std::string& model_name) override;
    bool UnloadModel(const std::string& model_name) override;
    bool IsModelLoaded(const std::string& model_name) const override;
    TaskResult RunInference(const std::string& model_name, const std::vector<TensorDesc>& inputs, std::vector<TensorDesc>& outputs) override;
    uint64_t RunInferenceAsync(const std::string& model_name, const std::vector<TensorDesc>& inputs, TaskCompleteCallback callback) override;
    bool HasError() const override { return has_error_.load(); }
    std::string GetLastError() const override { return last_error_; }
    void ResetError() override;

private:
    bool LoadOrtDll();
    void UnloadOrtDll();
    std::string FindModelFile(const std::string& model_name);

    HMODULE ort_dll_ = nullptr;
    bool initialized_ = false;
    std::atomic<bool> has_error_{false};
    std::string last_error_;
    std::atomic<size_t> used_memory_{0};

    // OrtApi vtable pointer (array of function pointers)
    const void* api_ = nullptr;

    // OrtEnv*
    void* ort_env_ = nullptr;

    struct ModelInfo {
        void* session = nullptr;
        size_t memory_usage = 0;
    };
    mutable std::mutex models_mutex_;
    std::unordered_map<std::string, ModelInfo> loaded_models_;
    std::atomic<uint64_t> next_async_id_{1};

    // Function pointer to OrtGetApiBase
    using Fn_OrtGetApiBase = void*(*)();
    Fn_OrtGetApiBase fn_get_api_base_ = nullptr;
};

} // namespace carbon

#endif // CARBON_PLATFORM_WINDOWS
