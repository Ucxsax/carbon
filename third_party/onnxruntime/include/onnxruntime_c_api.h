#pragma once
// Minimal ONNX Runtime C API header for Carbon
// Compatible with ORT 1.23.x - only declares what we use

#include <cstddef>
#include <cstdint>

// ============ ONNX Runtime C API Types ============

typedef struct OrtApi OrtApi;
typedef struct OrtEnv OrtEnv;
typedef struct OrtSession OrtSession;
typedef struct OrtSessionOptions OrtSessionOptions;
typedef struct OrtRunOptions OrtRunOptions;
typedef struct OrtValue OrtValue;
typedef struct OrtMemoryInfo OrtMemoryInfo;
typedef struct OrtArenaCfg OrtArenaCfg;
typedef struct OrtIoBinding OrtIoBinding;

typedef enum OrtLoggingLevel {
    ORT_LOGGING_LEVEL_VERBOSE = 0,
    ORT_LOGGING_LEVEL_INFO = 1,
    ORT_LOGGING_LEVEL_WARNING = 2,
    ORT_LOGGING_LEVEL_ERROR = 3,
    ORT_LOGGING_LEVEL_FATAL = 4
} OrtLoggingLevel;

typedef enum OrtMemType {
    ORT_MEMTYPE_CPU = 0,
    ORT_MEMTYPE_CPU_OUTPUT = ORT_MEMTYPE_CPU,
    ORT_MEMTYPE_CPU_INPUT = ORT_MEMTYPE_CPU,
    ORT_MEMTYPE_DEFAULT = 0,
    ORT_MEMTYPE_CUDA = 1,
    ORT_MEMTYPE_CUDA_INPUT = 1,
    ORT_MEMTYPE_CUDA_OUTPUT = 2
} OrtMemType;

typedef enum OrtAllocatorType {
    ORT_INVALIDAllocator = -1,
    ORTArenaAllocator = 0,
    ORTDeviceAllocator = 1
} OrtAllocatorType;

typedef enum OrtCustomOpInputOutputCharacteristic {
    ORT_CUSTOM_OP_INPUT_OUTPUT_VARIABLE = 0,
    ORT_CUSTOM_OP_INPUT_OUTPUT_FIXED = 1
} OrtCustomOpInputOutputCharacteristic;

typedef enum GraphOptimizationLevel {
    ORT_DISABLE_ALL = 0,
    ORT_ENABLE_BASIC = 1,
    ORT_ENABLE_EXTENDED = 2,
    ORT_ENABLE_ALL = 99
} GraphOptimizationLevel;

typedef enum OrtExecutionMode {
    ORT_SEQUENTIAL = 0,
    ORT_PARALLEL = 1
} OrtExecutionMode;

// OrtApi function pointer table (simplified)
struct OrtApi {
    size_t struct_size;
    void (*ReleaseStatus)(OrtStatus*);
    const OrtApi* (*GetApi)(uint32_t version);
    OrtStatus* (*CreateEnv)(OrtLoggingLevel, const char* logid, OrtEnv** env);
    OrtStatus* (*CreateEnvWithCustomLogger)(/*...*/);
    OrtStatus* (*EnableTelemetryEvents)(const OrtEnv* env);
    OrtStatus* (*DisableTelemetryEvents)(const OrtEnv* env);
    OrtStatus* (*CreateSession)(OrtEnv* env, const char* model_path, const OrtSessionOptions* options, OrtSession** session);
    OrtStatus* (*CreateSessionFromArray)(OrtEnv* env, const void* model_data, size_t model_data_length, const OrtSessionOptions* options, OrtSession** session);
    OrtStatus* (*Run)(OrtSession* session, const OrtRunOptions* run_options, const char* const* input_names, const OrtValue* const* inputs, size_t input_len, const char* const* output_names, size_t output_names_len, OrtValue** outputs);
    OrtStatus* (*CreateSessionOptions)(OrtSessionOptions** options);
    OrtStatus* (*SetOptimizedModelFilePath)(OrtSessionOptions* options, const char* optimized_model_file);
    OrtStatus* (*AppendExecutionProvider_CPU)(OrtSessionOptions* options, int use_arena);
    OrtStatus* (*GetAllocatorWithDefaultOptions)(OrtAllocator** allocator);
    OrtStatus* (*CreateRunOptions)(OrtRunOptions** run_options);
    // ... more functions, but we only need the ones above for CPU fallback
    // For DirectML, we need the execution provider API
};

typedef struct OrtStatus {
    OrtErrorCode code;
    char* message;
} OrtStatus;

typedef enum OrtErrorCode {
    ORT_OK = 0,
    ORT_FAIL = 1,
    ORT_INVALID_ARGUMENT = 2,
    ORT_NO_SUCHFILE = 3,
    ORT_NO_MODEL = 4,
    ORT_ENGINE_ERROR = 5,
    ORT_RUNTIME_EXCEPTION = 6,
    ORT_INVALID_PROTOBUF = 7,
    ORT_MODEL_LOADED = 8,
    ORT_NOT_IMPLEMENTED = 9,
    ORT_INVALID_GRAPH = 10,
    ORT_EP_FAIL = 11
} OrtErrorCode;

// Forward declarations for functions we actually use
#ifdef __cplusplus
extern "C" {
#endif

const OrtApi* OrtGetApiBase(void);

#ifdef __cplusplus
}
#endif
