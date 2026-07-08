#include "carbon/scheduler/task_queue.h"
#include "carbon/utils/logger.h"
#include <chrono>

namespace carbon {

uint64_t TaskQueue::Enqueue(std::shared_ptr<NPUTask> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutdown_) {
        return 0;
    }
    
    task->id = next_task_id_++;
    task->submit_time = 
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    
    queue_.push(task);
    priority_counts_[task->priority]++;
    
    cv_.notify_one();
    
    CARBON_LOG_DEBUG("Task enqueued: id=" + std::to_string(task->id) + 
                     ", priority=" + std::to_string(static_cast<int>(task->priority)));
    
    return task->id;
}

std::shared_ptr<NPUTask> TaskQueue::Dequeue() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    cv_.wait(lock, [this] {
        return !queue_.empty() || shutdown_;
    });
    
    if (shutdown_ && queue_.empty()) {
        return nullptr;
    }
    
    auto task = queue_.top();
    queue_.pop();
    priority_counts_[task->priority]--;
    
    return task;
}

std::shared_ptr<NPUTask> TaskQueue::TryDequeue() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        return nullptr;
    }
    
    auto task = queue_.top();
    queue_.pop();
    priority_counts_[task->priority]--;
    
    return task;
}

size_t TaskQueue::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

size_t TaskQueue::SizeByPriority(TaskPriority priority) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = priority_counts_.find(priority);
    return it != priority_counts_.end() ? it->second : 0;
}

void TaskQueue::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 清空优先队列
    while (!queue_.empty()) {
        queue_.pop();
    }
    priority_counts_.clear();
}

void TaskQueue::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = false;
    while (!queue_.empty()) {
        queue_.pop();
    }
    priority_counts_.clear();
    next_task_id_ = 1;
}

void TaskQueue::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();
}

bool TaskQueue::Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

} // namespace carbon
