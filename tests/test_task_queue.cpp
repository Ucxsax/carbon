#include "carbon/scheduler/task_queue.h"
#include "test_framework.h"
#include <thread>
#include <atomic>

using namespace carbon;

TEST_CASE(TaskQueue_EnqueueDequeue) {
    TaskQueue queue;
    
    auto task = std::make_shared<NPUTask>();
    task->priority = TaskPriority::Normal;
    
    uint64_t id = queue.Enqueue(task);
    REQUIRE(id > 0);
    REQUIRE(queue.Size() == 1);
    
    auto dequeued = queue.TryDequeue();
    REQUIRE(dequeued != nullptr);
    REQUIRE(dequeued->id == id);
    REQUIRE(queue.Empty());
    
    return true;
}

TEST_CASE(TaskQueue_PriorityOrder) {
    TaskQueue queue;
    
    // 低优先级先入队
    auto low_task = std::make_shared<NPUTask>();
    low_task->priority = TaskPriority::Low;
    queue.Enqueue(low_task);
    
    // 高优先级后入队
    auto high_task = std::make_shared<NPUTask>();
    high_task->priority = TaskPriority::Critical;
    queue.Enqueue(high_task);
    
    // 普通优先级
    auto normal_task = std::make_shared<NPUTask>();
    normal_task->priority = TaskPriority::Normal;
    queue.Enqueue(normal_task);
    
    REQUIRE(queue.Size() == 3);
    
    // 出队顺序应该是 Critical -> Normal -> Low
    auto first = queue.TryDequeue();
    REQUIRE(first->priority == TaskPriority::Critical);
    
    auto second = queue.TryDequeue();
    REQUIRE(second->priority == TaskPriority::Normal);
    
    auto third = queue.TryDequeue();
    REQUIRE(third->priority == TaskPriority::Low);
    
    REQUIRE(queue.Empty());
    return true;
}

TEST_CASE(TaskQueue_BlockingDequeue) {
    TaskQueue queue;
    std::atomic<bool> thread_started{false};
    std::atomic<bool> got_task{false};
    
    std::thread consumer([&]() {
        thread_started = true;
        auto task = queue.Dequeue();
        if (task) {
            got_task = true;
        }
    });
    
    // 等待消费者线程启动
    while (!thread_started) {
        std::this_thread::yield();
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(!got_task); // 还在阻塞等待
    
    // 入队一个任务
    auto task = std::make_shared<NPUTask>();
    queue.Enqueue(task);
    
    consumer.join();
    REQUIRE(got_task);
    
    return true;
}

TEST_CASE(TaskQueue_ShutdownWakesThreads) {
    TaskQueue queue;
    std::atomic<bool> exited{false};
    
    std::thread consumer([&]() {
        auto task = queue.Dequeue();
        exited = true;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    REQUIRE(!exited);
    
    queue.Shutdown();
    consumer.join();
    REQUIRE(exited);
    
    return true;
}

TEST_CASE(TaskQueue_PriorityCounts) {
    TaskQueue queue;
    
    auto t1 = std::make_shared<NPUTask>();
    t1->priority = TaskPriority::Critical;
    queue.Enqueue(t1);
    
    auto t2 = std::make_shared<NPUTask>();
    t2->priority = TaskPriority::Critical;
    queue.Enqueue(t2);
    
    auto t3 = std::make_shared<NPUTask>();
    t3->priority = TaskPriority::Low;
    queue.Enqueue(t3);
    
    REQUIRE(queue.SizeByPriority(TaskPriority::Critical) == 2);
    REQUIRE(queue.SizeByPriority(TaskPriority::Low) == 1);
    REQUIRE(queue.SizeByPriority(TaskPriority::Normal) == 0);
    
    return true;
}
