#include "carbon/hal/npu_backend.h"
#include "carbon/backends/mock_backend.h"
#include "carbon/backends/openvino_backend.h"
#include "carbon/backends/directml_backend.h"
#include "carbon/backends/tensorrt_backend.h"
#include "carbon/backends/ort_backend.h"
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
            backend = std::make_unique<OrtBackend>();
            break;
            
        case BackendType::CPU_Fallback:
            backend = std::make_unique<OrtBackend>();
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
    
    // OrtBackend 总是可用 (CPU EP is built into onnxruntime.dll)
    {
        auto backend = std::make_unique<OrtBackend>();
        if (backend->IsAvailable()) {
            backends.push_back(BackendType::DirectML);
            CARBON_LOG_INFO("ONNX Runtime backend available (CPU)");
        }
    }
    
    return backends;
}

} // namespace carbon
