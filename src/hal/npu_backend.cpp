#include "carbon/hal/npu_backend.h"
#include "carbon/backends/mock_backend.h"
#include "carbon/utils/logger.h"

namespace carbon {

std::unique_ptr<NPUBackend> BackendFactory::CreateBackend(BackendType type) {
    switch (type) {
        case BackendType::Mock:
            return std::make_unique<MockBackend>();
            
        case BackendType::OpenVINO:
            CARBON_LOG_WARN("OpenVINO backend not implemented yet, falling back to Mock");
            return std::make_unique<MockBackend>();
            
        case BackendType::TensorRT:
            CARBON_LOG_WARN("TensorRT backend not implemented yet, falling back to Mock");
            return std::make_unique<MockBackend>();
            
        case BackendType::DirectML:
            CARBON_LOG_WARN("DirectML backend not implemented yet, falling back to Mock");
            return std::make_unique<MockBackend>();
            
        case BackendType::CPU_Fallback:
            CARBON_LOG_WARN("CPU fallback backend not implemented yet, falling back to Mock");
            return std::make_unique<MockBackend>();
            
        default:
            return nullptr;
    }
}

std::vector<BackendType> BackendFactory::GetAvailableBackends() {
    // 目前只有Mock后端可用
    std::vector<BackendType> backends = { BackendType::Mock };
    
    // TODO: 检测实际硬件并添加对应后端
    // - 检测Intel NPU -> OpenVINO
    // - 检测NVIDIA GPU -> TensorRT
    // - Windows平台 -> DirectML
    
    return backends;
}

} // namespace carbon
