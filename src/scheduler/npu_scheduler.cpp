#include "carbon/scheduler/npu_scheduler.h"
#include "carbon/utils/logger.h"
#include "carbon/backends/mock_backend.h"
#include "carbon/hal/npu_backend.h"
#include <chrono>

namespace carbon {

NPUScheduler& NPUScheduler::Instance() {
    static NPUScheduler instance;
    return instance;
}

NPUScheduler::~NPUScheduler() {
    if (initialized_) {
        Shutdown();
    }
}

bool NPUScheduler::Initialize(const SchedulerConfig& config) {
    if (initialized_) {
        CARBON_LOG_WARN("Scheduler already initialized");
        return true;
    }
    
    config_ = config;
    
    // 重置任务队列
    task_queue_.Reset();
    
    if (!config.enable_npu_acceleration) {
        CARBON_LOG_INFO("NPU acceleration disabled, scheduler running in passthrough mode");
        initialized_ = true;
        return true;
    }
    
    running_ = true;
    
    // 自动检测可用后端，按优先级注册
    auto available = BackendFactory::GetAvailableBackends();
    bool registered_any = false;
    
    for (auto type : available) {
        auto backend = BackendFactory::CreateBackend(type);
        if (backend) {
            RegisterBackend(std::move(backend));
            registered_any = true;
        }
    }
    
    // 兜底：如果没有任何后端可用，注册 Mock
    if (!registered_any) {
        CARBON_LOG_WARN("No real backends available, falling back to MockBackend");
        auto mock_backend = std::make_unique<MockBackend>();
        if (mock_backend->Initialize()) {
            RegisterBackend(std::move(mock_backend));
        }
    }
    
    CARBON_LOG_INFO("NPU Scheduler initialized with " + 
                    std::to_string(backends_.size()) + " backends");
    
    initialized_ = true;
    return true;
}

void NPUScheduler::Shutdown() {
    if (!initialized_) return;
    
    running_ = false;
    task_queue_.Shutdown();
    
    // 停止所有工作线程
    {
        std::lock_guard<std::mutex> lock(backends_mutex_);
        for (auto& state : backends_) {
            state->is_running = false;
            if (state->worker_thread.joinable()) {
                state->worker_thread.join();
            }
            state->backend->Shutdown();
        }
        backends_.clear();
    }
    
    task_map_.clear();
    initialized_ = false;
    
    CARBON_LOG_INFO("NPU Scheduler shutdown complete");
}

uint64_t NPUScheduler::SubmitTask(std::shared_ptr<NPUTask> task) {
    if (!initialized_ || !config_.enable_npu_acceleration) {
        CARBON_LOG_WARN("NPU acceleration disabled, task rejected");
        return 0;
    }
    
    // 存入任务映射
    {
        std::lock_guard<std::mutex> lock(task_map_mutex_);
        task_map_[task->id] = task;
    }
    
    uint64_t task_id = task_queue_.Enqueue(task);
    return task_id;
}

std::vector<uint64_t> NPUScheduler::SubmitBatch(
    const std::vector<std::shared_ptr<NPUTask>>& tasks
) {
    std::vector<uint64_t> ids;
    ids.reserve(tasks.size());
    
    for (const auto& task : tasks) {
        ids.push_back(SubmitTask(task));
    }
    
    return ids;
}

bool NPUScheduler::CancelTask(uint64_t task_id) {
    std::lock_guard<std::mutex> lock(task_map_mutex_);
    auto it = task_map_.find(task_id);
    if (it != task_map_.end()) {
        // 注意：已经在执行的任务无法取消
        // 这里只是标记，实际需要更复杂的取消机制
        task_map_.erase(it);
        return true;
    }
    return false;
}

size_t NPUScheduler::GetPendingTaskCount() const {
    return task_queue_.Size();
}

size_t NPUScheduler::GetRunningTaskCount() const {
    return running_tasks_.load();
}

float NPUScheduler::GetAverageUtilization() const {
    std::lock_guard<std::mutex> lock(backends_mutex_);
    if (backends_.empty()) return 0.0f;
    
    float total = 0.0f;
    for (const auto& state : backends_) {
        total += state->backend->GetUtilization();
    }
    return total / static_cast<float>(backends_.size());
}

bool NPUScheduler::RegisterBackend(std::unique_ptr<NPUBackend> backend) {
    auto state = std::make_unique<BackendWorkerState>();
    state->backend = std::move(backend);
    state->is_running = true;
    
    // 启动工作线程
    state->worker_thread = std::thread(&NPUScheduler::WorkerLoop, this, state.get());
    
    std::lock_guard<std::mutex> lock(backends_mutex_);
    backends_.push_back(std::move(state));
    
    CARBON_LOG_INFO("Backend registered: " + backends_.back()->backend->GetName());
    return true;
}

void NPUScheduler::UnregisterBackend(BackendType type) {
    std::lock_guard<std::mutex> lock(backends_mutex_);
    
    for (auto it = backends_.begin(); it != backends_.end(); ++it) {
        if ((*it)->backend->GetType() == type) {
            (*it)->is_running = false;
            if ((*it)->worker_thread.joinable()) {
                (*it)->worker_thread.join();
            }
            (*it)->backend->Shutdown();
            backends_.erase(it);
            break;
        }
    }
}

std::vector<BackendType> NPUScheduler::GetActiveBackends() const {
    std::lock_guard<std::mutex> lock(backends_mutex_);
    std::vector<BackendType> types;
    for (const auto& state : backends_) {
        types.push_back(state->backend->GetType());
    }
    return types;
}

void NPUScheduler::SetAccelerationEnabled(bool enabled) {
    config_.enable_npu_acceleration = enabled;
    CARBON_LOG_INFO(std::string("NPU acceleration ") + (enabled ? "enabled" : "disabled"));
}

void NPUScheduler::UpdateConfig(const SchedulerConfig& config) {
    config_ = config;
}

// ========== 私有方法 ==========

void NPUScheduler::WorkerLoop(BackendWorkerState* state) {
    CARBON_LOG_DEBUG("Worker thread started for " + state->backend->GetName());
    
    while (state->is_running && running_) {
        // 检查熔断
        if (config_.enable_circuit_breaker && IsCircuitBroken(state)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 获取任务
        auto task = DispatchTask(state);
        if (!task) {
            // 队列空，短暂等待
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // 执行任务
        ExecuteTask(state, task);
    }
    
    CARBON_LOG_DEBUG("Worker thread stopped for " + state->backend->GetName());
}

std::shared_ptr<NPUTask> NPUScheduler::DispatchTask(BackendWorkerState* state) {
    // 检查负载 - 如果NPU占用过高，不接受新任务
    if (config_.enable_auto_degrade && ShouldDegradeToCPU(state)) {
        return nullptr;
    }
    
    return task_queue_.TryDequeue();
}

void NPUScheduler::ExecuteTask(BackendWorkerState* state, std::shared_ptr<NPUTask> task) {
    running_tasks_++;
    task->assigned_backend = state->backend->GetType();
    task->start_time = 
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    
    // 执行推理
    TaskResult result = state->backend->RunInference(
        task->model_name,
        task->inputs,
        task->outputs
    );
    
    // 记录成功/失败
    if (result.success) {
        RecordSuccess(state);
    } else {
        RecordFailure(state);
        CARBON_LOG_ERROR("Task " + std::to_string(task->id) + 
                         " failed: " + result.error_message);
    }
    
    // 回调
    if (task->callback) {
        task->callback(task->id, result);
    }
    
    // 清理任务映射
    {
        std::lock_guard<std::mutex> lock(task_map_mutex_);
        task_map_.erase(task->id);
    }
    
    running_tasks_--;
}

bool NPUScheduler::ShouldDegradeToCPU(const BackendWorkerState* state) const {
    return state->backend->GetUtilization() > config_.npu_high_utilization_threshold;
}

bool NPUScheduler::ShouldExpandBatch(const BackendWorkerState* state) const {
    // CPU占用高时，扩大NPU批量任务规模
    // 这里简化处理，实际需要监控CPU单线程占用率
    return state->backend->GetUtilization() < 0.5f;
}

bool NPUScheduler::IsCircuitBroken(BackendWorkerState* state) const {
    if (state->consecutive_failures >= config_.max_consecutive_failures) {
        uint64_t now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        
        if (now < state->circuit_breaker_until.load()) {
            return true;
        }
        // 冷却期过了，重置
        state->consecutive_failures = 0;
    }
    return false;
}

void NPUScheduler::RecordFailure(BackendWorkerState* state) {
    size_t failures = ++(state->consecutive_failures);
    
    if (failures >= config_.max_consecutive_failures) {
        auto cooldown_until = 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count() + config_.circuit_breaker_cooldown_ms;
        
        state->circuit_breaker_until = cooldown_until;
        
        CARBON_LOG_WARN("Circuit breaker triggered for " + state->backend->GetName() +
                        ", cooldown " + std::to_string(config_.circuit_breaker_cooldown_ms) + "ms");
    }
}

void NPUScheduler::RecordSuccess(BackendWorkerState* state) {
    state->consecutive_failures = 0;
}

} // namespace carbon
