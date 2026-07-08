#pragma once

#include "carbon/common_types.h"
#include "carbon/scheduler/task_queue.h"
#include "carbon/hal/npu_backend.h"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace carbon {

// 调度器配置
struct SchedulerConfig {
    // 负载降级阈值
    float npu_high_utilization_threshold = 0.85f;  // NPU占用率 > 85% 降级
    float cpu_single_thread_threshold = 0.90f;     // CPU单线程 > 90% 扩容
    
    // 线程池配置
    size_t worker_thread_count = 2;
    
    // 自动降级开关
    bool enable_auto_degrade = true;
    
    // 熔断机制
    bool enable_circuit_breaker = true;
    size_t max_consecutive_failures = 5;
    uint64_t circuit_breaker_cooldown_ms = 5000;
    
    // 全局开关
    bool enable_npu_acceleration = true;
};

// 后端工作状态
struct BackendWorkerState {
    std::unique_ptr<NPUBackend> backend;
    std::thread worker_thread;
    std::atomic<bool> is_running{false};
    std::atomic<size_t> consecutive_failures{0};
    std::atomic<uint64_t> circuit_breaker_until{0};
};

// NPU 统一调度器
class NPUScheduler {
public:
    static NPUScheduler& Instance();
    
    ~NPUScheduler();
    
    // 初始化/关闭
    bool Initialize(const SchedulerConfig& config = SchedulerConfig{});
    void Shutdown();
    bool IsInitialized() const { return initialized_; }
    
    // 提交任务
    uint64_t SubmitTask(std::shared_ptr<NPUTask> task);
    
    // 批量提交
    std::vector<uint64_t> SubmitBatch(const std::vector<std::shared_ptr<NPUTask>>& tasks);
    
    // 取消任务
    bool CancelTask(uint64_t task_id);
    
    // 状态查询
    size_t GetPendingTaskCount() const;
    size_t GetRunningTaskCount() const;
    float GetAverageUtilization() const;
    
    // 后端管理
    bool RegisterBackend(std::unique_ptr<NPUBackend> backend);
    void UnregisterBackend(BackendType type);
    std::vector<BackendType> GetActiveBackends() const;
    
    // 开关控制
    void SetAccelerationEnabled(bool enabled);
    bool IsAccelerationEnabled() const { return config_.enable_npu_acceleration; }
    
    // 配置更新
    void UpdateConfig(const SchedulerConfig& config);
    
private:
    NPUScheduler() = default;
    
    // 工作线程主循环
    void WorkerLoop(BackendWorkerState* state);
    
    // 任务分发
    std::shared_ptr<NPUTask> DispatchTask(BackendWorkerState* state);
    
    // 执行任务
    void ExecuteTask(BackendWorkerState* state, std::shared_ptr<NPUTask> task);
    
    // 负载均衡判断
    bool ShouldDegradeToCPU(const BackendWorkerState* state) const;
    bool ShouldExpandBatch(const BackendWorkerState* state) const;
    
    // 熔断检查
    bool IsCircuitBroken(BackendWorkerState* state) const;
    void RecordFailure(BackendWorkerState* state);
    void RecordSuccess(BackendWorkerState* state);
    
    // 成员
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    SchedulerConfig config_;
    
    TaskQueue task_queue_;
    
    mutable std::mutex backends_mutex_;
    std::vector<std::unique_ptr<BackendWorkerState>> backends_;
    
    std::atomic<size_t> running_tasks_{0};
    
    // 任务ID映射
    mutable std::mutex task_map_mutex_;
    std::unordered_map<uint64_t, std::shared_ptr<NPUTask>> task_map_;
};

} // namespace carbon
