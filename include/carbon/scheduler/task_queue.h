#pragma once

#include "carbon/common_types.h"
#include "carbon/hal/npu_backend.h"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>

namespace carbon {

// 任务描述
struct NPUTask {
    uint64_t id = 0;
    TaskType type = TaskType::Custom;
    TaskPriority priority = TaskPriority::Normal;
    std::string model_name;
    std::vector<TensorDesc> inputs;
    std::vector<TensorDesc> outputs;
    TaskCompleteCallback callback;
    
    // 任务元数据
    uint64_t submit_time = 0;
    uint64_t start_time = 0;
    BackendType assigned_backend = BackendType::Mock;
};

// 优先级比较（小顶堆，优先级数值越小越优先）
struct TaskPriorityCompare {
    bool operator()(const std::shared_ptr<NPUTask>& a, const std::shared_ptr<NPUTask>& b) const {
        if (a->priority == b->priority) {
            return a->id > b->id; // FIFO for same priority
        }
        return static_cast<uint8_t>(a->priority) > static_cast<uint8_t>(b->priority);
    }
};

// 线程安全的优先级任务队列
class TaskQueue {
public:
    TaskQueue() = default;
    ~TaskQueue() = default;
    
    // 入队
    uint64_t Enqueue(std::shared_ptr<NPUTask> task);
    
    // 出队（阻塞）
    std::shared_ptr<NPUTask> Dequeue();
    
    // 尝试出队（非阻塞）
    std::shared_ptr<NPUTask> TryDequeue();
    
    // 队列大小
    size_t Size() const;
    size_t SizeByPriority(TaskPriority priority) const;
    
    // 清空
    void Clear();
    
    // 重置（重新启用队列）
    void Reset();
    
    // 关闭队列（唤醒所有等待线程）
    void Shutdown();
    
    // 是否为空
    bool Empty() const;
    
private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<std::shared_ptr<NPUTask>, 
                        std::vector<std::shared_ptr<NPUTask>>,
                        TaskPriorityCompare> queue_;
    std::atomic<uint64_t> next_task_id_{1};
    std::atomic<bool> shutdown_{false};
    
    // 各优先级计数
    std::unordered_map<TaskPriority, size_t> priority_counts_;
};

} // namespace carbon
