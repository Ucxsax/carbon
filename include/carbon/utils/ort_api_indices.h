// OrtApi function pointer index mapping for ORT 1.23.x (ORT_API_VERSION=12)
// Confirmed by runtime dump of onnxruntime.dll
#pragma once

namespace carbon {
namespace ort_idx {

// Session
constexpr size_t CreateEnv = 3;
constexpr size_t CreateSession = 7;
constexpr size_t CreateSessionFromArray = 8;
constexpr size_t Run = 9;

// Session options
constexpr size_t CreateSessionOptions = 10;
constexpr size_t SetSessionGraphOptimizationLevel = 23;
constexpr size_t SetIntraOpNumThreads = 24;
constexpr size_t SetInterOpNumThreads = 25;

// Session info
constexpr size_t SessionGetInputCount = 30;
constexpr size_t SessionGetOutputCount = 31;
constexpr size_t SessionGetInputName = 36;
constexpr size_t SessionGetOutputName = 37;

// Run options
constexpr size_t CreateRunOptions = 39;

// Tensor creation
constexpr size_t CreateTensorAsOrtValue = 48;
constexpr size_t CreateTensorWithDataAsOrtValue = 49;
constexpr size_t IsTensor = 50;
constexpr size_t GetTensorMutableData = 51;

// Type info
constexpr size_t GetTensorElementType = 60;
constexpr size_t GetDimensionsCount = 61;
constexpr size_t GetDimensions = 62;
constexpr size_t GetTensorTypeAndShape = 65;
constexpr size_t GetTypeInfo = 66;

// Memory info
constexpr size_t CreateCpuMemoryInfo = 69;

// Allocator
constexpr size_t GetAllocatorWithDefaultOptions = 78;

// Release functions
constexpr size_t ReleaseEnv = 92;
constexpr size_t ReleaseStatus = 93;
constexpr size_t ReleaseMemoryInfo = 94;
constexpr size_t ReleaseSession = 95;
constexpr size_t ReleaseValue = 96;
constexpr size_t ReleaseRunOptions = 97;
constexpr size_t ReleaseTypeInfo = 98;
constexpr size_t ReleaseTensorTypeAndShapeInfo = 99;
constexpr size_t ReleaseSessionOptions = 100;

// CPU EP
constexpr size_t SessionOptionsAppendExecutionProvider_CUDA = 152;
constexpr size_t SessionOptionsAppendExecutionProvider_TensorRT = 159;
constexpr size_t SessionOptionsAppendExecutionProvider_TensorRT_V2 = 170;

} // namespace ort_idx
} // namespace carbon
