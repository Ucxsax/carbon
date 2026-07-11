#pragma once
// ONNX Runtime dynamic loader for Carbon
// Loads ORT at runtime via LoadLibrary/GetProcAddress

#ifdef CARBON_PLATFORM_WINDOWS

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <functional>
#include <windows.h>

// ORT C API forward declarations
typedef struct OrtEnv OrtEnv;
typedef struct OrtSession OrtSession;
typedef struct OrtSessionOptions OrtSessionOptions;
typedef struct OrtRunOptions OrtRunOptions;
typedef struct OrtValue OrtValue;
typedef struct OrtMemoryInfo OrtMemoryInfo;
typedef struct OrtAllocator OrtAllocator;

enum OrtLoggingLevel {
    ORT_LOGGING_LEVEL_VERBOSE = 0,
    ORT_LOGGING_LEVEL_INFO = 1,
    ORT_LOGGING_LEVEL_WARNING = 2,
    ORT_LOGGING_LEVEL_ERROR = 3,
    ORT_LOGGING_LEVEL_FATAL = 4
};

enum OrtErrorCode {
    ORT_OK = 0, ORT_FAIL = 1
};

enum OrtMemType {
    ORT_MEMTYPE_CPU = 0,
    ORT_MEMTYPE_CPU_INPUT = 0,
    ORT_MEMTYPE_CPU_OUTPUT = 0
};

enum OrtAllocatorType {
    ORT_INVALIDAllocator = -1,
    ORTArenaAllocator = 0,
    ORTDeviceAllocator = 1
};

enum GraphOptimizationLevel {
    ORT_DISABLE_ALL = 0,
    ORT_ENABLE_BASIC = 1,
    ORT_ENABLE_EXTENDED = 2,
    ORT_ENABLE_ALL = 99
};

enum OrtExecutionMode {
    ORT_SEQUENTIAL = 0,
    ORT_PARALLEL = 1
};

struct OrtApi;
struct OrtStatus {
    OrtErrorCode code;
    void* message;
};

// Minimal OrtApi function pointer table (matches ORT 1.x layout)
typedef const OrtApi* (*OrtGetApiBaseFunc)();

// We only declare the function pointer types we actually call
struct OrtApi {
    size_t struct_size;
    void (*ReleaseStatus)(OrtStatus*);
    const OrtApi* (*GetApi)(uint32_t version);
    OrtStatus* (*CreateEnv)(OrtLoggingLevel, const char* logid, OrtEnv** env);
    // ... skip to the ones we need (offsets matter!)
    // For simplicity, we'll call via the actual OrtApi pointer from GetApi()
};

namespace carbon {

class OrtDynamicLoader {
public:
    static OrtDynamicLoader& Instance();

    bool LoadLibrary(const std::string& dll_path = "onnxruntime.dll");
    void UnloadLibrary();
    bool IsLoaded() const { return loaded_; }

    // Get the OrtApi function pointer table
    const OrtApi* GetApi() const { return api_; }

    // Convenience: call GetApiBase to get the api
    using GetApiBaseFunc = const OrtApi* (*)();
    GetApiBaseFunc GetApiBaseFuncPtr() const { return get_api_base_; }

    std::string GetLastError() const { return last_error_; }

private:
    OrtDynamicLoader() = default;
    ~OrtDynamicLoader();

    HMODULE hmodule_ = nullptr;
    bool loaded_ = false;
    const OrtApi* api_ = nullptr;
    GetApiBaseFunc get_api_base_ = nullptr;
    std::string last_error_;
};

} // namespace carbon

#endif // CARBON_PLATFORM_WINDOWS
