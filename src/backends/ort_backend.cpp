#include "carbon/backends/ort_backend.h"
#include "carbon/utils/logger.h"
#include "carbon/utils/ort_api_indices.h"
#include <chrono>
#include <cstring>
#include <thread>
#include <filesystem>

#ifdef CARBON_PLATFORM_WINDOWS

namespace carbon {

// ===== ORT C API function pointer types =====
// Match the exact signatures from onnxruntime_c_api.h
using Fn_CreateEnv = void*(*)(int /*OrtLoggingLevel*/, const char*, void**);
using Fn_CreateSessionOptions = void*(*)();
using Fn_SetIntraOpNumThreads = void*(*)(void*, int);
using Fn_SetSessionGraphOptimizationLevel = void*(*)(void*, int);
using Fn_CreateSession = void*(*)(void*, const wchar_t*, void*, void**);
using Fn_SessionGetInputCount = void*(*)(void*, size_t*);
using Fn_SessionGetOutputCount = void*(*)(void*, size_t*);
using Fn_SessionGetInputName = void*(*)(void*, size_t, void*, void**);
using Fn_SessionGetOutputName = void*(*)(void*, size_t, void*, void**);
using Fn_GetAllocatorWithDefaultOptions = void*(*)();
using Fn_CreateTensorAsOrtValue = void*(*)(void*, const int64_t*, size_t, int /*ONNXTensorElementDataType*/, void**);
using Fn_CreateTensorWithDataAsOrtValue = void*(*)(void*, void*, size_t, const int64_t*, size_t, int, void**);
using Fn_CreateRunOptions = void*(*)();
using Fn_Run = void*(*)(void*, void*, const char* const*, const void* const*, size_t, const char* const*, size_t, void**);
using Fn_IsTensor = int(*)(const void*);
using Fn_GetTensorMutableData = void*(*)(void*, void**);
using Fn_GetTensorTypeAndShape = void*(*)(const void*, void**);
using Fn_GetTensorElementType = void*(*)(void*, int*);
using Fn_GetDimensionsCount = void*(*)(void*, size_t*);
using Fn_GetDimensions = void*(*)(void*, int64_t*, size_t);
using Fn_ReleaseEnv = void(*)(void*);
using Fn_ReleaseSession = void(*)(void*);
using Fn_ReleaseSessionOptions = void(*)(void*);
using Fn_ReleaseRunOptions = void(*)(void*);
using Fn_ReleaseValue = void(*)(void*);
using Fn_ReleaseStatus = void(*)(void*);
using Fn_ReleaseMemoryInfo = void(*)(void*);
using Fn_ReleaseTypeInfo = void(*)(void*);
using Fn_ReleaseTensorTypeAndShapeInfo = void(*)(void*);
using Fn_GetTypeInfo = void*(*)(const void*, void**);
using Fn_GetOnnxTypeFromTypeInfo = void*(*)(void*, int*);
using Fn_CreateCpuMemoryInfo = void*(*)(int, int, void**);
using Fn_GetExecutionProviderApi = void*(*)(const char*, uint32_t, const void**);

// OrtGetApiBase
struct OrtApiBase {
    void* get_api;
    void* get_version_string;
};
using Fn_OrtGetApiBase = OrtApiBase*(*)();

OrtBackend::OrtBackend() = default;

OrtBackend::~OrtBackend() {
    if (initialized_) Shutdown();
}

bool OrtBackend::IsAvailable() const {
    // Check if onnxruntime.dll exists alongside the mod
    return true; // Always available - CPU EP is built-in
}

bool OrtBackend::LoadOrtDll() {
    if (ort_dll_) return true;

    // Try loading from mod directory first, then system PATH
    std::wstring dll_name = L"onnxruntime.dll";

    // Try current directory
    ort_dll_ = LoadLibraryW(dll_name.c_str());

    if (!ort_dll_) {
        // Try alongside the current module
        wchar_t module_path[MAX_PATH];
        GetModuleFileNameW(NULL, module_path, MAX_PATH);
        std::wstring dir = std::wstring(module_path);
        size_t pos = dir.find_last_of(L'\\');
        if (pos != std::wstring::npos) {
            dir = dir.substr(0, pos + 1);
            ort_dll_ = LoadLibraryW((dir + dll_name).c_str());
        }
    }

    if (!ort_dll_) {
        last_error_ = "Failed to load onnxruntime.dll";
        return false;
    }

    fn_get_api_base_ = (Fn_OrtGetApiBase)GetProcAddress(ort_dll_, "OrtGetApiBase");
    if (!fn_get_api_base_) {
        last_error_ = "OrtGetApiBase not found in onnxruntime.dll";
        return false;
    }

    // Get the API vtable
    OrtApiBase* api_base = reinterpret_cast<OrtApiBase*>(fn_get_api_base_());
    if (!api_base || !api_base->get_api) {
        last_error_ = "Failed to get OrtApi";
        return false;
    }

    // Call GetApi(ORT_API_VERSION=12 for ORT 1.23.x)
    using Fn_GetApi = void*(*)(uint32_t);
    Fn_GetApi get_api = (Fn_GetApi)api_base->get_api;
    api_ = get_api(12);
    if (!api_) {
        last_error_ = "GetApi returned null for version 12";
        return false;
    }

    CARBON_LOG_INFO("Loaded onnxruntime.dll successfully");
    return true;
}

void OrtBackend::UnloadOrtDll() {
    if (ort_dll_) {
        FreeLibrary(ort_dll_);
        ort_dll_ = nullptr;
    }
    api_ = nullptr;
    fn_get_api_base_ = nullptr;
}

bool OrtBackend::Initialize() {
    if (initialized_) return true;

    if (!LoadOrtDll()) return false;

    // Create ORT environment
    const void** api_table = (const void**)api_;
    auto fn_create_env = (Fn_CreateEnv)api_table[ort_idx::CreateEnv];
    void* status = fn_create_env(2 /*WARNING*/, "Carbon", &ort_env_);
    if (status) {
        last_error_ = "Failed to create ORT environment";
        auto fn_release_status = (Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus];
        fn_release_status(status);
        return false;
    }

    initialized_ = true;
    CARBON_LOG_INFO("OrtBackend initialized");
    return true;
}

void OrtBackend::Shutdown() {
    if (!initialized_) return;

    // Release all sessions
    {
        std::lock_guard<std::mutex> lock(models_mutex_);
        const void** api_table = (const void**)api_;
        auto fn_release_session = (Fn_ReleaseSession)api_table[ort_idx::ReleaseSession];

        for (auto& [name, info] : loaded_models_) {
            if (info.session) {
                fn_release_session(info.session);
            }
        }
        loaded_models_.clear();
    }

    // Release env
    if (ort_env_) {
        const void** api_table = (const void**)api_;
        auto fn_release_env = (Fn_ReleaseEnv)api_table[ort_idx::ReleaseEnv];
        fn_release_env(ort_env_);
        ort_env_ = nullptr;
    }

    UnloadOrtDll();
    initialized_ = false;
    used_memory_ = 0;
    CARBON_LOG_INFO("OrtBackend shutdown");
}

float OrtBackend::GetUtilization() const { return 0.0f; }

size_t OrtBackend::GetTotalMemory() const {
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        return ms.ullTotalPhys;
    }
    return 4ULL * 1024 * 1024 * 1024;
}

size_t OrtBackend::GetUsedMemory() const { return used_memory_.load(); }

std::string OrtBackend::FindModelFile(const std::string& model_name) {
    std::string full_name = model_name;
    if (full_name.find(".onnx") == std::string::npos) {
        full_name += ".onnx";
    }

    // Try relative paths from mod directory
    const char* search_paths[] = {
        "models/",
        "assets/",
        "",
    };

    for (const char* prefix : search_paths) {
        std::string path = std::string(prefix) + full_name;
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return full_name; // Return as-is, will fail with a clear error
}

bool OrtBackend::LoadModel(const std::string& model_path, const std::string& model_name) {
    if (!initialized_) return false;

    std::lock_guard<std::mutex> lock(models_mutex_);

    if (loaded_models_.count(model_name) > 0) return true;

    const void** api_table = (const void**)api_;

    // Create session options
    auto fn_create_session_opts = (Fn_CreateSessionOptions)api_table[ort_idx::CreateSessionOptions];
    void* session_opts = fn_create_session_opts();
    if (!session_opts) {
        last_error_ = "Failed to create session options";
        return false;
    }

    // Configure
    auto fn_set_threads = (Fn_SetIntraOpNumThreads)api_table[ort_idx::SetIntraOpNumThreads];
    fn_set_threads(session_opts, 1);  // Single thread for low latency

    auto fn_set_opt_level = (Fn_SetSessionGraphOptimizationLevel)api_table[ort_idx::SetSessionGraphOptimizationLevel];
    fn_set_opt_level(session_opts, 3);  // ORT_ENABLE_ALL

    // Create session
    auto fn_create_session = (Fn_CreateSession)api_table[ort_idx::CreateSession];
    std::string model_path_str = FindModelFile(model_name);
    std::wstring wmodel_path(model_path_str.begin(), model_path_str.end());

    void* session = nullptr;
    void* status = fn_create_session(ort_env_, wmodel_path.c_str(), session_opts, &session);

    // Release session options
    auto fn_release_opts = (Fn_ReleaseSessionOptions)api_table[ort_idx::ReleaseSessionOptions];
    fn_release_opts(session_opts);

    if (status) {
        auto fn_release_status = (Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus];
        const char* msg = "Session creation failed";
        // Try to get error message
        using Fn_GetErrorMessage = const char*(*)(void*);
        auto fn_get_err = (Fn_GetErrorMessage)api_table[1]; // GetErrorCode is 1, GetErrorMessage is 2
        auto fn_get_err_msg = (Fn_GetErrorMessage)api_table[2];
        msg = fn_get_err_msg(status);
        last_error_ = std::string("Failed to create session: ") + (msg ? msg : "unknown");
        fn_release_status(status);
        return false;
    }

    size_t memory_usage = 10 * 1024 * 1024; // Estimate 10MB per model
    loaded_models_[model_name] = {session, memory_usage};
    used_memory_ += memory_usage;

    CARBON_LOG_INFO("Loaded ONNX model: " + model_name);
    return true;
}

bool OrtBackend::UnloadModel(const std::string& model_name) {
    std::lock_guard<std::mutex> lock(models_mutex_);
    auto it = loaded_models_.find(model_name);
    if (it == loaded_models_.end()) return false;

    const void** api_table = (const void**)api_;
    auto fn_release_session = (Fn_ReleaseSession)api_table[ort_idx::ReleaseSession];
    if (it->second.session) fn_release_session(it->second.session);
    used_memory_ -= it->second.memory_usage;
    loaded_models_.erase(it);
    return true;
}

bool OrtBackend::IsModelLoaded(const std::string& model_name) const {
    std::lock_guard<std::mutex> lock(models_mutex_);
    return loaded_models_.count(model_name) > 0;
}

TaskResult OrtBackend::RunInference(
    const std::string& model_name,
    const std::vector<TensorDesc>& inputs,
    std::vector<TensorDesc>& outputs
) {
    TaskResult result;
    auto start = std::chrono::steady_clock::now();

    if (!initialized_) {
        result.success = false;
        result.error_message = "Backend not initialized";
        return result;
    }

    const void** api_table = (const void**)api_;

    std::lock_guard<std::mutex> lock(models_mutex_);
    auto it = loaded_models_.find(model_name);
    if (it == loaded_models_.end()) {
        result.success = false;
        result.error_message = "Model not loaded: " + model_name;
        return result;
    }

    void* session = it->second.session;

    try {
        // Get input/output count
        auto fn_get_input_count = (Fn_SessionGetInputCount)api_table[ort_idx::SessionGetInputCount];
        auto fn_get_output_count = (Fn_SessionGetOutputCount)api_table[ort_idx::SessionGetOutputCount];

        size_t num_inputs = 0, num_outputs = 0;
        void* status;

        status = fn_get_input_count(session, &num_inputs);
        if (status) { ((Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus])(status); }

        status = fn_get_output_count(session, &num_outputs);
        if (status) { ((Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus])(status); }

        // Build input names and values
        auto fn_get_input_name = (Fn_SessionGetInputName)api_table[ort_idx::SessionGetInputName];
        auto fn_get_allocator = (Fn_GetAllocatorWithDefaultOptions)api_table[ort_idx::GetAllocatorWithDefaultOptions];
        void* allocator = fn_get_allocator();

        std::vector<std::string> input_names_storage(num_inputs);
        std::vector<const char*> input_names(num_inputs);
        std::vector<void*> ort_values(num_inputs, nullptr);

        for (size_t i = 0; i < num_inputs && i < inputs.size(); i++) {
            char* name = nullptr;
            status = fn_get_input_name(session, i, allocator, reinterpret_cast<void**>(&name));
            if (status) {
                ((Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus])(status);
                continue;
            }
            input_names_storage[i] = name ? name : "";
            input_names[i] = input_names_storage[i].c_str();

            // Create tensor from input data
            if (inputs[i].data && inputs[i].size_bytes > 0) {
                auto fn_create_tensor = (Fn_CreateTensorWithDataAsOrtValue)api_table[ort_idx::CreateTensorWithDataAsOrtValue];

                // Determine ONNX data type
                int onnx_type = 1; // FLOAT
                switch (inputs[i].data_type) {
                    case TensorDesc::DataType::Float32: onnx_type = 1; break;
                    case TensorDesc::DataType::Float16: onnx_type = 10; break;
                    case TensorDesc::DataType::Int8: onnx_type = 3; break;
                    case TensorDesc::DataType::Int32: onnx_type = 6; break;
                    case TensorDesc::DataType::UInt8: onnx_type = 2; break;
                }

                status = fn_create_tensor(
                    allocator,
                    inputs[i].data,
                    inputs[i].size_bytes,
                    inputs[i].shape.data(),
                    inputs[i].shape.size(),
                    onnx_type,
                    &ort_values[i]
                );
                if (status) {
                    ((Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus])(status);
                }
            }
        }

        // Get output names
        auto fn_get_output_name = (Fn_SessionGetOutputName)api_table[ort_idx::SessionGetOutputName];
        std::vector<std::string> output_names_storage(num_outputs);
        std::vector<const char*> output_names(num_outputs);

        for (size_t i = 0; i < num_outputs; i++) {
            char* name = nullptr;
            status = fn_get_output_name(session, i, allocator, reinterpret_cast<void**>(&name));
            if (status) {
                ((Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus])(status);
                continue;
            }
            output_names_storage[i] = name ? name : "";
            output_names[i] = output_names_storage[i].c_str();
        }

        // Allocate output pointers
        std::vector<void*> output_values(num_outputs, nullptr);

        // Run inference
        auto fn_run = (Fn_Run)api_table[ort_idx::Run];
        status = fn_run(
            session,
            nullptr,  // default run options
            input_names.data(),
            (const void* const*)ort_values.data(),
            num_inputs,
            output_names.data(),
            num_outputs,
            output_values.data()
        );

        if (status) {
            auto fn_release_status = (Fn_ReleaseStatus)api_table[ort_idx::ReleaseStatus];
            const char* msg = "Inference failed";
            using Fn_GetErrorMessage = const char*(*)(void*);
            auto fn_get_err_msg = (Fn_GetErrorMessage)api_table[2];
            msg = fn_get_err_msg(status);
            result.success = false;
            result.error_message = std::string("Inference error: ") + (msg ? msg : "unknown");
            fn_release_status(status);
        } else {
            // Copy outputs to TensorDesc
            auto fn_get_tensor_data = (Fn_GetTensorMutableData)api_table[ort_idx::GetTensorMutableData];
            auto fn_get_tensor_type_shape = (Fn_GetTensorTypeAndShape)api_table[ort_idx::GetTensorTypeAndShape];
            auto fn_get_tensor_elem_type = (Fn_GetTensorElementType)api_table[ort_idx::GetTensorElementType];
            auto fn_get_dims_count = (Fn_GetDimensionsCount)api_table[ort_idx::GetDimensionsCount];
            auto fn_get_dims = (Fn_GetDimensions)api_table[ort_idx::GetDimensions];
            auto fn_release_tts = (Fn_ReleaseTensorTypeAndShapeInfo)api_table[ort_idx::ReleaseTensorTypeAndShapeInfo];
            auto fn_release_value = (Fn_ReleaseValue)api_table[ort_idx::ReleaseValue];

            for (size_t i = 0; i < num_outputs && i < outputs.size(); i++) {
                void* ort_val = output_values[i];
                if (!ort_val) continue;

                // Get shape info
                void* type_shape = nullptr;
                status = fn_get_tensor_type_shape(ort_val, &type_shape);
                if (!status && type_shape) {
                    int elem_type = 0;
                    fn_get_tensor_elem_type(type_shape, &elem_type);

                    size_t ndim = 0;
                    fn_get_dims_count(type_shape, &ndim);

                    std::vector<int64_t> shape(ndim);
                    fn_get_dims(type_shape, shape.data(), ndim);

                    outputs[i].shape = shape;
                    outputs[i].data_type = TensorDesc::DataType::Float32; // Assume float for now

                    // Get data pointer
                    void* data_ptr = nullptr;
                    fn_get_tensor_data(ort_val, &data_ptr);

                    // Calculate size
                    size_t elem_count = 1;
                    for (auto d : shape) elem_count *= d;
                    outputs[i].size_bytes = elem_count * sizeof(float);
                    outputs[i].data = data_ptr;

                    fn_release_tts(type_shape);
                }
            }
            result.success = true;
        }

        // Clean up input ort values
        auto fn_release_value = (Fn_ReleaseValue)api_table[ort_idx::ReleaseValue];
        for (auto v : ort_values) {
            if (v) fn_release_value(v);
        }
        for (auto v : output_values) {
            if (v) fn_release_value(v);
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        last_error_ = e.what();
        has_error_ = true;
    }

    auto end = std::chrono::steady_clock::now();
    result.execution_time_us =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    return result;
}

uint64_t OrtBackend::RunInferenceAsync(
    const std::string& model_name,
    const std::vector<TensorDesc>& inputs,
    TaskCompleteCallback callback
) {
    uint64_t task_id = next_async_id_++;
    std::thread([this, task_id, model_name, inputs, callback]() {
        std::vector<TensorDesc> outputs;
        TaskResult result = RunInference(model_name, inputs, outputs);
        if (callback) callback(task_id, result);
    }).detach();
    return task_id;
}

void OrtBackend::ResetError() {
    has_error_ = false;
    last_error_.clear();
}

} // namespace carbon

#endif // CARBON_PLATFORM_WINDOWS
