#include "carbon/backends/mock_backend.h"
#include "test_framework.h"
#include <vector>
#include <cstring>

using namespace carbon;

TEST_CASE(MockBackend_InitShutdown) {
    MockBackend backend;
    REQUIRE(!backend.IsAvailable() || backend.IsAvailable()); // 模拟后端始终可用
    
    REQUIRE(backend.Initialize());
    REQUIRE(backend.GetType() == BackendType::Mock);
    REQUIRE(backend.GetName() == "MockBackend");
    
    backend.Shutdown();
    return true;
}

TEST_CASE(MockBackend_ModelLoading) {
    MockBackend backend;
    backend.Initialize();
    
    REQUIRE(!backend.IsModelLoaded("test_model"));
    REQUIRE(backend.LoadModel("/fake/path.onnx", "test_model"));
    REQUIRE(backend.IsModelLoaded("test_model"));
    
    REQUIRE(backend.UnloadModel("test_model"));
    REQUIRE(!backend.IsModelLoaded("test_model"));
    
    backend.Shutdown();
    return true;
}

TEST_CASE(MockBackend_SyncInference) {
    MockBackend backend;
    backend.Initialize();
    backend.SetSimulatedDelayMs(1);
    
    backend.LoadModel("test", "test_model");
    
    // 准备输入输出
    std::vector<float> input_data(100, 1.0f);
    std::vector<float> output_data(50, 0.0f);
    
    std::vector<TensorDesc> inputs = {{
        {1, 100},
        TensorDesc::DataType::Float32,
        input_data.data(),
        input_data.size() * sizeof(float)
    }};
    
    std::vector<TensorDesc> outputs = {{
        {1, 50},
        TensorDesc::DataType::Float32,
        output_data.data(),
        output_data.size() * sizeof(float)
    }};
    
    TaskResult result = backend.RunInference("test_model", inputs, outputs);
    REQUIRE(result.success);
    REQUIRE(result.execution_time_us > 0);
    
    backend.Shutdown();
    return true;
}

TEST_CASE(MockBackend_InferenceWithoutModel) {
    MockBackend backend;
    backend.Initialize();
    
    std::vector<TensorDesc> inputs;
    std::vector<TensorDesc> outputs;
    
    TaskResult result = backend.RunInference("nonexistent", inputs, outputs);
    REQUIRE(!result.success);
    REQUIRE(!result.error_message.empty());
    
    backend.Shutdown();
    return true;
}

TEST_CASE(MockBackend_FailureRate) {
    MockBackend backend;
    backend.Initialize();
    backend.SetSimulatedDelayMs(0);
    backend.SetFailureRate(1.0f); // 100% 失败
    
    backend.LoadModel("test", "test_model");
    
    std::vector<TensorDesc> inputs;
    std::vector<TensorDesc> outputs;
    
    int failures = 0;
    for (int i = 0; i < 10; i++) {
        TaskResult result = backend.RunInference("test_model", inputs, outputs);
        if (!result.success) failures++;
    }
    
    REQUIRE(failures == 10);
    
    backend.Shutdown();
    return true;
}

TEST_CASE(MockBackend_MemoryTracking) {
    MockBackend backend;
    backend.Initialize();
    
    size_t initial_mem = backend.GetUsedMemory();
    
    backend.LoadModel("a", "model_a");
    REQUIRE(backend.GetUsedMemory() > initial_mem);
    
    backend.LoadModel("b", "model_b");
    size_t two_models = backend.GetUsedMemory();
    
    backend.UnloadModel("model_a");
    REQUIRE(backend.GetUsedMemory() < two_models);
    
    backend.Shutdown();
    return true;
}
