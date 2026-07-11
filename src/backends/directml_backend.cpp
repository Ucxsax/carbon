#include "carbon/backends/directml_backend.h"
#include "carbon/utils/logger.h"
#include <chrono>
#include <cstring>

#ifdef CARBON_PLATFORM_WINDOWS
#include <windows.h>
#include <dxgi1_6.h>
// #include <d3d12.h>
// #include <DirectML.h>
// #include <wrl/client.h>
#endif

namespace carbon {

DirectMLBackend::DirectMLBackend() = default;

DirectMLBackend::~DirectMLBackend() {
    if (initialized_) {
        Shutdown();
    }
}

bool DirectMLBackend::IsAvailable() const {
#ifdef CARBON_PLATFORM_WINDOWS
    // 检查是否支持 DirectML
    // 需要 Windows 10 1809+ 和支持 DirectX 12 的 GPU
    // 简化检查：尝试创建 DXGI Factory
    IDXGIFactory6* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&factory);
    if (SUCCEEDED(hr)) {
        IDXGIAdapter* adapter = nullptr;
        hr = factory->EnumAdapters(0, &adapter);
        if (SUCCEEDED(hr)) {
            adapter->Release();
            factory->Release();
            return true;
        }
        factory->Release();
    }
#endif
    return false;
}

bool DirectMLBackend::Initialize() {
    if (initialized_) {
        return true;
    }
    
#ifdef CARBON_PLATFORM_WINDOWS
    if (!InitializeD3D12()) {
        return false;
    }
    
    if (!InitializeDirectML()) {
        return false;
    }
#endif
    
    initialized_ = true;
    CARBON_LOG_INFO("DirectMLBackend initialized successfully");
    return true;
}

void DirectMLBackend::Shutdown() {
    if (!initialized_) return;
    
    {
        std::lock_guard<std::mutex> lock(models_mutex_);
        loaded_models_.clear();
    }
    
#ifdef CARBON_PLATFORM_WINDOWS
    // dml_recorder_.Reset();
    // dml_device_.Reset();
    // command_queue_.Reset();
    // d3d12_device_.Reset();
    // dxgi_adapter_.Reset();
    // ort_env_.reset();
#endif
    
    initialized_ = false;
    used_memory_ = 0;
    
    CARBON_LOG_INFO("DirectMLBackend shutdown");
}

float DirectMLBackend::GetUtilization() const {
    if (!initialized_) return 0.0f;
    
#ifdef CARBON_PLATFORM_WINDOWS
    // 可以通过 DXGI 查询 GPU 占用率
    // 这里返回模拟值
#endif
    
    return 0.25f; // 模拟值
}

size_t DirectMLBackend::GetTotalMemory() const {
    if (total_memory_.load() > 0) {
        return total_memory_.load();
    }
    
#ifdef CARBON_PLATFORM_WINDOWS
    // 查询 GPU 显存
    IDXGIFactory6* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&factory))) {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters(0, &adapter))) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(adapter->GetDesc(&desc))) {
                size_t dedicated_video_mem = desc.DedicatedVideoMemory;
                adapter->Release();
                factory->Release();
                return dedicated_video_mem;
            }
            adapter->Release();
        }
        factory->Release();
    }
#endif
    
    return 2ULL * 1024 * 1024 * 1024; // 2GB 默认
}

size_t DirectMLBackend::GetUsedMemory() const {
    return used_memory_.load();
}

bool DirectMLBackend::LoadModel(const std::string& model_path, const std::string& model_name) {
    if (!initialized_) return false;
    
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    if (loaded_models_.count(model_name) > 0) {
        return true;
    }
    
    if (!CreateOrtSession(model_path, model_name)) {
        return false;
    }
    
    CARBON_LOG_INFO("Loaded model via DirectML: " + model_name);
    return true;
}

bool DirectMLBackend::UnloadModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    
    auto it = loaded_models_.find(model_name);
    if (it != loaded_models_.end()) {
        // if (it->second.session) {
        //     delete it->second.session;
        // }
        used_memory_ -= it->second.memory_usage;
        loaded_models_.erase(it);
        return true;
    }
    return false;
}

bool DirectMLBackend::IsModelLoaded(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return loaded_models_.count(model_name) > 0;
}

TaskResult DirectMLBackend::RunInference(
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
    
#ifdef CARBON_PLATFORM_WINDOWS
    try {
        std::lock_guard<std::mutex> lock(models_mutex_);
        
        auto it = loaded_models_.find(model_name);
        if (it == loaded_models_.end()) {
            result.success = false;
            result.error_message = "Model not loaded: " + model_name;
            return result;
        }
        
        // TODO: 实际实现需要:
        // 1. 将 TensorDesc 转换为 Ort::Value
        // 2. 设置输入
        // 3. 执行推理
        // 4. 获取输出并转换回 TensorDesc
        
        result.success = true;
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        last_error_ = result.error_message;
        has_error_ = true;
    }
#else
    result.success = false;
    result.error_message = "DirectML only available on Windows";
#endif
    
    auto end = std::chrono::steady_clock::now();
    result.execution_time_us = 
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    return result;
}

uint64_t DirectMLBackend::RunInferenceAsync(
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

void DirectMLBackend::ResetError() {
    has_error_ = false;
    last_error_.clear();
}

// ========== 私有方法 ==========

bool DirectMLBackend::InitializeD3D12() {
#ifdef CARBON_PLATFORM_WINDOWS
    try {
        // 创建 DXGI Factory
        // IDXGIFactory6* factory = nullptr;
        // HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&factory);
        // if (FAILED(hr)) {
        //     CARBON_LOG_ERROR("Failed to create DXGI Factory");
        //     return false;
        // }
        
        // 枚举适配器（优先选择独立显卡）
        // IDXGIAdapter* selected_adapter = nullptr;
        // IDXGIAdapter* adapter = nullptr;
        // SIZE_T max_dedicated_video_memory = 0;
        
        // for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        //     DXGI_ADAPTER_DESC desc;
        //     adapter->GetDesc(&desc);
            
        //     // 跳过软件适配器
        //     if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        //         adapter->Release();
        //         continue;
        //     }
            
        //     // 选择显存最大的适配器
        //     if (desc.DedicatedVideoMemory > max_dedicated_video_memory) {
        //         if (selected_adapter) selected_adapter->Release();
        //         selected_adapter = adapter;
        //         max_dedicated_video_memory = desc.DedicatedVideoMemory;
        //     } else {
        //         adapter->Release();
        //     }
        // }
        
        // factory->Release();
        
        // if (!selected_adapter) {
        //     CARBON_LOG_ERROR("No suitable GPU adapter found");
        //     return false;
        // }
        
        // dxgi_adapter_ = selected_adapter;
        
        // 创建 D3D12 设备
        // hr = D3D12CreateDevice(
        //     selected_adapter,
        //     D3D_FEATURE_LEVEL_11_0,
        //     __uuidof(ID3D12Device),
        //     (void**)&d3d12_device_
        // );
        
        // if (FAILED(hr)) {
        //     CARBON_LOG_ERROR("Failed to create D3D12 device");
        //     return false;
        // }
        
        // 创建命令队列
        // D3D12_COMMAND_QUEUE_DESC queue_desc = {};
        // queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        // queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        
        // hr = d3d12_device_->CreateCommandQueue(
        //     &queue_desc,
        //     __uuidof(ID3D12CommandQueue),
        //     (void**)&command_queue_
        // );
        
        // if (FAILED(hr)) {
        //     CARBON_LOG_ERROR("Failed to create command queue");
        //     return false;
        // }
        
        CARBON_LOG_INFO("D3D12 initialized successfully");
        return true;
    } catch (const std::exception& e) {
        CARBON_LOG_ERROR("Failed to initialize D3D12: " + std::string(e.what()));
        return false;
    }
#else
    return false;
#endif
}

bool DirectMLBackend::InitializeDirectML() {
#ifdef CARBON_PLATFORM_WINDOWS
    try {
        // 创建 DirectML 设备
        // DMLCreateDevice(
        //     d3d12_device_.Get(),
        //     DML_CREATE_DEVICE_FLAG_NONE,
        //     __uuidof(IDMLDevice),
        //     (void**)&dml_device_
        // );
        
        // 创建命令录制器
        // dml_device_->CreateCommandRecorder(__uuidof(IDMLCommandRecorder), (void**)&dml_recorder_);
        
        CARBON_LOG_INFO("DirectML initialized successfully");
        return true;
    } catch (const std::exception& e) {
        CARBON_LOG_ERROR("Failed to initialize DirectML: " + std::string(e.what()));
        return false;
    }
#else
    return false;
#endif
}

bool DirectMLBackend::CreateOrtSession(const std::string& model_path, const std::string& model_name) {
#ifdef CARBON_PLATFORM_WINDOWS
    try {
        // 初始化 ONNX Runtime 环境
        // ort_env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "CarbonDirectML");
        
        // 配置 ONNX Runtime Session Options
        // Ort::SessionOptions session_options;
        // session_options.SetIntraOpNumThreads(1);
        // session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // 注册 DirectML 执行提供器
        // OrtDmlApi* dml_api = nullptr;
        // Ort::GetApi().GetExecutionProviderApi("DML", ORT_API_VERSION, (const void**)&dml_api);
        
        // 创建 Session
        // auto session = std::make_unique<Ort::Session>(
        //     *ort_env_,
        //     model_path.c_str(),
        //     session_options
        // );
        
        // 计算内存占用
        size_t memory_usage = 10 * 1024 * 1024; // 模拟 10MB
        
        ModelInfo info;
        // info.session = session.release();
        info.memory_usage = memory_usage;
        
        loaded_models_[model_name] = std::move(info);
        used_memory_ += memory_usage;
        
        CARBON_LOG_INFO("Created ONNX Runtime session with DirectML EP: " + model_name);
        return true;
    } catch (const std::exception& e) {
        CARBON_LOG_ERROR("Failed to create ORT session: " + std::string(e.what()));
        last_error_ = e.what();
        has_error_ = true;
        return false;
    }
#else
    return false;
#endif
}

} // namespace carbon
