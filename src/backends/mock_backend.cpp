#include "carbon/backends/mock_backend.h"
#include "carbon/utils/logger.h"
#include <thread>
#include <random>
#include <cstring>

namespace carbon {

MockBackend::MockBackend() = default;

MockBackend::~MockBackend() {
    if (initialized_) {
        Shutdown();
    }
}

bool MockBackend::Initialize() {
    initialized_ = true;
    
    // 自动加载 P0 内置模型
    LoadModel("", "chunk_mesh_v1");
    LoadModel("", "light_baking_v1");
    
    CARBON_LOG_INFO("MockBackend initialized");
    return true;
}

void MockBackend::Shutdown() {
    initialized_ = false;
    std::lock_guard<std::mutex> lock(models_mutex_);
    loaded_models_.clear();
    used_memory_ = 0;
    CARBON_LOG_INFO("MockBackend shutdown");
}

float MockBackend::GetUtilization() const {
    return simulated_util_.load();
}

size_t MockBackend::GetUsedMemory() const {
    return used_memory_.load();
}

bool MockBackend::LoadModel(const std::string& model_path, const std::string& model_name) {
    if (!initialized_) return false;
    
    std::lock_guard<std::mutex> lock(models_mutex_);
    loaded_models_[model_name] = true;
    used_memory_ += 10 * 1024 * 1024; // 模拟每个模型占10MB
    
    CARBON_LOG_DEBUG("MockBackend loaded model: " + model_name);
    return true;
}

bool MockBackend::UnloadModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    auto it = loaded_models_.find(model_name);
    if (it != loaded_models_.end()) {
        loaded_models_.erase(it);
        used_memory_ -= 10 * 1024 * 1024;
        return true;
    }
    return false;
}

bool MockBackend::IsModelLoaded(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return loaded_models_.count(model_name) > 0;
}

TaskResult MockBackend::RunInference(
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
    
    if (!IsModelLoaded(model_name)) {
        result.success = false;
        result.error_message = "Model not loaded: " + model_name;
        return result;
    }
    
    // 模拟计算延迟
    auto start = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(simulated_delay_ms_));
    auto end = std::chrono::steady_clock::now();
    
    result.execution_time_us = 
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // 模拟失败率
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    if (dist(gen) < failure_rate_) {
        result.success = false;
        result.error_message = "Simulated inference failure";
        last_error_ = result.error_message;
        return result;
    }
    
    // 模拟输出 - 用0填充输出张量
    for (auto& output : outputs) {
        if (output.data && output.size_bytes > 0) {
            std::memset(output.data, 0, output.size_bytes);
        }
    }
    
    result.success = true;
    return result;
}

uint64_t MockBackend::RunInferenceAsync(
    const std::string& model_name,
    const std::vector<TensorDesc>& inputs,
    TaskCompleteCallback callback
) {
    uint64_t task_id = next_async_id_++;
    
    // 简单的异步：启动一个线程执行
    std::thread([this, task_id, model_name, inputs, callback]() {
        std::vector<TensorDesc> outputs; // 模拟输出
        TaskResult result = RunInference(model_name, inputs, outputs);
        if (callback) {
            callback(task_id, result);
        }
    }).detach();
    
    return task_id;
}

} // namespace carbon
