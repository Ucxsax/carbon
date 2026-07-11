#include "carbon/backends/openvino_backend.h"
#include "carbon/utils/logger.h"
#include <chrono>
#include <cstring>

// Windows-specific headers for memory info
#ifdef CARBON_PLATFORM_WINDOWS
#include <windows.h>
#include <psapi.h>
#endif

namespace carbon {

OpenVINOBackend::OpenVINOBackend() = default;

OpenVINOBackend::~OpenVINOBackend() {
    if (initialized_) {
        Shutdown();
    }
}

bool OpenVINOBackend::IsAvailable() const {
#ifdef CARBON_USE_OPENVINO
    try {
        ov::Core core;
        auto devices = core.get_available_devices();
        for (const auto& device : devices) {
            if (device.find("NPU") != std::string::npos) {
                return true;
            }
        }
    } catch (...) {
        return false;
    }
#endif
    return false;
}

bool OpenVINOBackend::Initialize() {
    if (initialized_) {
        return true;
    }
    
    if (!InitializeOpenVINO()) {
        return false;
    }
    
    if (!DetectIntelNPU()) {
        CARBON_LOG_WARN("Intel NPU not detected, OpenVINO backend may use CPU fallback");
    }
    
    initialized_ = true;
    CARBON_LOG_INFO("OpenVINOBackend initialized successfully");
    return true;
}

void OpenVINOBackend::Shutdown() {
    if (!initialized_) return;
    
    {
        std::lock_guard<std::mutex> lock(models_mutex_);
        loaded_models_.clear();
    }
    
#ifdef CARBON_USE_OPENVINO
    infer_request_.reset();
    current_model_ = {};
    ov_core_.reset();
#endif
    
    initialized_ = false;
    used_memory_ = 0;
    
    CARBON_LOG_INFO("OpenVINOBackend shutdown");
}

float OpenVINOBackend::GetUtilization() const {
    // 实际实现需要查询 Intel NPU 驱动获取占用率
    // 这里返回模拟值
    if (!initialized_) return 0.0f;
    
#ifdef CARBON_PLATFORM_WINDOWS
    // 尝试通过 Performance Counter 获取 NPU 占用
    // 实际需要 Intel NPU 驱动支持
#endif
    
    return 0.3f; // 模拟值
}

size_t OpenVINOBackend::GetTotalMemory() const {
    if (total_memory_.load() > 0) {
        return total_memory_.load();
    }
    
    // 尝试查询 Intel NPU 显存
    // 这里返回默认值
    return 4ULL * 1024 * 1024 * 1024; // 4GB 默认
}

size_t OpenVINOBackend::GetUsedMemory() const {
    return used_memory_.load();
}

bool OpenVINOBackend::LoadModel(const std::string& model_path, const std::string& model_name) {
    if (!initialized_) return false;
    
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    if (loaded_models_.count(model_name) > 0) {
        CARBON_LOG_DEBUG("Model already loaded: " + model_name);
        return true;
    }
    
    if (!CompileModel(model_path, model_name)) {
        return false;
    }
    
    CARBON_LOG_INFO("Loaded model: " + model_name);
    return true;
}

bool OpenVINOBackend::UnloadModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = loaded_models_.find(model_name);
    if (it != loaded_models_.end()) {
        used_memory_ -= it->second.memory_usage;
        loaded_models_.erase(it);
        CARBON_LOG_INFO("Unloaded model: " + model_name);
        return true;
    }
    return false;
}

bool OpenVINOBackend::IsModelLoaded(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return loaded_models_.count(model_name) > 0;
}

TaskResult OpenVINOBackend::RunInference(
    const std::string& model_name,
    const std::vector<TensorDesc>& inputs,
    std::vector<TensorDesc>& outputs
) {
    TaskResult result;
    
    if (!initialized_) {
        result.success = false;
        result.error_message = "Backend not initialized";
        return result;
    }
    
    auto start = std::chrono::steady_clock::now();
    
#ifdef CARBON_USE_OPENVINO
    try {
        std::lock_guard<std::mutex> lock(models_mutex_);
        
        auto it = loaded_models_.find(model_name);
        if (it == loaded_models_.end()) {
            result.success = false;
            result.error_message = "Model not loaded: " + model_name;
            return result;
        }
        
        // 设置输入
        auto& infer_request = it->second.infer_request;
        if (!infer_request) {
            result.success = false;
            result.error_message = "Infer request not created";
            return result;
        }
        
        // TODO: 实际实现需要将 TensorDesc 转换为 ov::Tensor
        // 这里是框架代码
        
        // 执行推理
        infer_request->infer();
        
        // 获取输出
        // TODO: 实际实现需要将 ov::Tensor 转换回 TensorDesc
        
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        last_error_ = result.error_message;
        has_error_ = true;
    }
#else
    result.success = false;
    result.error_message = "OpenVINO not compiled";
#endif
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time_us = 
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    return result;
}

uint64_t OpenVINOBackend::RunInferenceAsync(
    const std::string& model_name,
    const std::vector<TensorDesc>& inputs,
    TaskCompleteCallback callback
) {
    uint64_t task_id = next_async_id_++;
    
    std::thread([this, task_id, model_name, inputs, callback]() {
        std::vector<TensorDesc> outputs;
        TaskResult result = RunInference(model_name, inputs, outputs);
        if (callback) {
            callback(task_id, result);
        }
    }).detach();
    
    return task_id;
}

void OpenVINOBackend::ResetError() {
    has_error_ = false;
    last_error_.clear();
}

// ========== 私有方法 ==========

bool OpenVINOBackend::InitializeOpenVINO() {
#ifdef CARBON_USE_OPENVINO
    try {
        ov_core_ = std::make_unique<ov::Core>();
        
        // 获取可用设备列表
        auto devices = ov_core_->get_available_devices();
        CARBON_LOG_INFO("OpenVINO available devices:");
        for (const auto& device : devices) {
            CARBON_LOG_INFO("  - " + device);
        }
        
        return true;
    } catch (const std::exception& e) {
        CARBON_LOG_ERROR("Failed to initialize OpenVINO: " + std::string(e.what()));
        last_error_ = e.what();
        has_error_ = true;
        return false;
    }
#else
    CARBON_LOG_WARN("OpenVINO not compiled, backend unavailable");
    return false;
#endif
}

bool OpenVINOBackend::DetectIntelNPU() {
#ifdef CARBON_USE_OPENVINO
    if (!ov_core_) return false;
    
    try {
        auto devices = ov_core_->get_available_devices();
        for (const auto& device : devices) {
            if (device.find("NPU") != std::string::npos) {
                CARBON_LOG_INFO("Intel NPU detected: " + device);
                
                // 查询设备信息
                auto max_streams = ov_core_->get_property(device, ov::optimal_number_of_infer_requests);
                CARBON_LOG_INFO("Optimal infer requests: " + std::to_string(max_streams));
                
                return true;
            }
        }
    } catch (const std::exception& e) {
        CARBON_LOG_WARN("Failed to detect Intel NPU: " + std::string(e.what()));
    }
#endif
    return false;
}

bool OpenVINOBackend::CompileModel(const std::string& model_path, const std::string& model_name) {
#ifdef CARBON_USE_OPENVINO
    if (!ov_core_) return false;
    
    try {
        // 加载模型
        auto model = ov_core_->read_model(model_path);
        
        // 优先使用 NPU，否则使用 CPU
        std::string target_device = "CPU";
        auto devices = ov_core_->get_available_devices();
        for (const auto& device : devices) {
            if (device.find("NPU") != std::string::npos) {
                target_device = device;
                break;
            }
        }
        
        CARBON_LOG_INFO("Compiling model '" + model_name + "' to device: " + target_device);
        
        // 编译模型
        auto compiled_model = ov_core_->compile_model(model, target_device);
        
        // 创建推理请求
        auto infer_request = compiled_model.create_infer_request();
        
        // 计算模型内存占用
        size_t memory_usage = 0;
        // 简化估算
        for (const auto& param : model->get_parameters()) {
            auto shape = param->get_shape();
            size_t param_size = 1;
            for (auto dim : shape) {
                param_size *= dim;
            }
            memory_usage += param_size * sizeof(float); // 假设 float32
        }
        
        ModelInfo info;
        // info.model = model;
        // info.compiled_model = std::make_shared<ov::CompiledModel>(std::move(compiled_model));
        // info.infer_request = std::make_unique<ov::InferRequest>(std::move(infer_request));
        info.memory_usage = memory_usage;
        
        loaded_models_[model_name] = std::move(info);
        used_memory_ += memory_usage;
        
        return true;
    } catch (const std::exception& e) {
        CARBON_LOG_ERROR("Failed to compile model '" + model_name + "': " + std::string(e.what()));
        last_error_ = e.what();
        has_error_ = true;
        return false;
    }
#else
    CARBON_LOG_WARN("OpenVINO not compiled, cannot compile model");
    return false;
#endif
}

} // namespace carbon
