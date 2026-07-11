#include "carbon/hal/npu_backend.h"
#include "carbon/backends/mock_backend.h"
#include "carbon/backends/openvino_backend.h"
#include "carbon/backends/directml_backend.h"
#include "carbon/backends/tensorrt_backend.h"
#include "carbon/utils/logger.h"

namespace carbon {

std::unique_ptr<NPUBackend> BackendFactory::CreateBackend(BackendType type) {
    std::unique_ptr<NPUBackend> backend;
    
    switch (type) {
        case BackendType::Mock:
            backend = std::make_unique<MockBackend>();
            break;
            
        case BackendType::OpenVINO:
            backend = std::make_unique<OpenVINOBackend>();
            break;
            
        case BackendType::TensorRT:
            backend = std::make_unique<TensorRTBackend>();
            break;
            
        case BackendType::DirectML:
            backend = std::make_unique<DirectMLBackend>();
            break;
            
        case BackendType::CPU_Fallback:
            CARBON_LOG_WARN("CPU fallback not implemented, using Mock backend");
            backend = std::make_unique<MockBackend>();
            break;
            
        default:
            return nullptr;
    }
    
    // 初始化后端
    if (backend && !backend->Initialize()) {
        CARBON_LOG_WARN("Failed to initialize backend: " + backend->GetName());
        return nullptr;
    }
    
    return backend;
}

std::vector<BackendType> BackendFactory::GetAvailableBackends() {
    std::vector<BackendType> backends;
    
    // Mock 后端总是可用
    backends.push_back(BackendType::Mock);
    
    // 检测 OpenVINO (Intel NPU)
#ifdef CARBON_USE_OPENVINO
    {
        auto backend = std::make_unique<OpenVINOBackend>();
        if (backend->IsAvailable()) {
            backends.push_back(BackendType::OpenVINO);
            CARBON_LOG_INFO("Intel NPU detected via OpenVINO");
        }
    }
#endif
    
    // 检测 TensorRT (NVIDIA)
#ifdef CARBON_USE_TENSORRT
    {
        auto backend = std::make_unique<TensorRTBackend>();
        if (backend->IsAvailable()) {
            backends.push_back(BackendType::TensorRT);
            CARBON_LOG_INFO("NVIDIA Tensor Core detected via TensorRT");
        }
    }
#endif
    
    // 检测 DirectML (AMD/Generic Windows)
#ifdef CARBON_USE_DIRECTML
    {
        auto backend = std::make_unique<DirectMLBackend>();
        if (backend->IsAvailable()) {
            backends.push_back(BackendType::DirectML);
            CARBON_LOG_INFO("DirectML backend detected");
        }
    }
#endif
    
    return backends;
}

} // namespace carbon
