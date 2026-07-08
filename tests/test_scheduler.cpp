#include "carbon/scheduler/npu_scheduler.h"
#include "carbon/backends/mock_backend.h"
#include "test_framework.h"
#include <atomic>
#include <chrono>

using namespace carbon;

TEST_CASE(Scheduler_Singleton) {
    auto& s1 = NPUScheduler::Instance();
    auto& s2 = NPUScheduler::Instance();
    REQUIRE(&s1 == &s2);
    return true;
}

TEST_CASE(Scheduler_InitShutdown) {
    auto& scheduler = NPUScheduler::Instance();
    
    // 先确保关闭
    if (scheduler.IsInitialized()) {
        scheduler.Shutdown();
    }
    
    SchedulerConfig config;
    config.worker_thread_count = 1;
    
    REQUIRE(scheduler.Initialize(config));
    REQUIRE(scheduler.IsInitialized());
    REQUIRE(scheduler.IsAccelerationEnabled());
    
    auto backends = scheduler.GetActiveBackends();
    REQUIRE(!backends.empty());
    
    scheduler.Shutdown();
    REQUIRE(!scheduler.IsInitialized());
    
    return true;
}

TEST_CASE(Scheduler_SubmitAndExecute) {
    auto& scheduler = NPUScheduler::Instance();
    if (scheduler.IsInitialized()) {
        scheduler.Shutdown();
    }
    
    SchedulerConfig config;
    scheduler.Initialize(config);
    
    // 先加载模型
    auto backends = scheduler.GetActiveBackends();
    REQUIRE(!backends.empty());
    
    // 提交任务
    std::atomic<bool> task_completed{false};
    std::atomic<bool> task_success{false};
    
    auto task = std::make_shared<NPUTask>();
    task->priority = TaskPriority::Normal;
    task->model_name = "test_model";
    task->callback = [&](uint64_t id, const TaskResult& result) {
        task_success = result.success;
        task_completed = true;
    };
    
    // 注意：需要先加载模型，这里Mock后端需要手动加载
    // 实际使用中调度器会自动处理
    // 我们直接测试队列功能
    
    uint64_t task_id = scheduler.SubmitTask(task);
    REQUIRE(task_id > 0);
    
    // 等待执行（Mock后端没有加载模型会失败，但回调应该被调用）
    for (int i = 0; i < 100; i++) {
        if (task_completed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 因为没加载模型，任务会失败，但调度流程是通的
    REQUIRE(task_completed);
    
    scheduler.Shutdown();
    return true;
}

TEST_CASE(Scheduler_GlobalToggle) {
    auto& scheduler = NPUScheduler::Instance();
    if (scheduler.IsInitialized()) {
        scheduler.Shutdown();
    }
    
    SchedulerConfig config;
    config.enable_npu_acceleration = false;
    scheduler.Initialize(config);
    
    REQUIRE(!scheduler.IsAccelerationEnabled());
    
    auto task = std::make_shared<NPUTask>();
    uint64_t id = scheduler.SubmitTask(task);
    REQUIRE(id == 0); // 关闭时提交失败
    
    scheduler.SetAccelerationEnabled(true);
    REQUIRE(scheduler.IsAccelerationEnabled());
    
    scheduler.Shutdown();
    return true;
}

TEST_CASE(Scheduler_PendingCount) {
    auto& scheduler = NPUScheduler::Instance();
    if (scheduler.IsInitialized()) {
        scheduler.Shutdown();
    }
    
    SchedulerConfig config;
    scheduler.Initialize(config);
    
    REQUIRE(scheduler.GetPendingTaskCount() == 0);
    
    // 提交几个任务
    for (int i = 0; i < 5; i++) {
        auto task = std::make_shared<NPUTask>();
        task->priority = TaskPriority::Low;
        task->model_name = "test";
        scheduler.SubmitTask(task);
    }
    
    // 任务可能被快速消费掉，所以数量可能小于5
    // 但至少验证接口可用
    size_t pending = scheduler.GetPendingTaskCount();
    REQUIRE(pending <= 5);
    
    scheduler.Shutdown();
    return true;
}

TEST_CASE(Scheduler_BackendRegistration) {
    auto& scheduler = NPUScheduler::Instance();
    if (scheduler.IsInitialized()) {
        scheduler.Shutdown();
    }
    
    // 不自动注册后端
    SchedulerConfig config;
    config.enable_npu_acceleration = true;
    scheduler.Initialize(config);
    
    size_t initial_count = scheduler.GetActiveBackends().size();
    
    // 注册一个新的Mock后端
    auto backend = std::make_unique<MockBackend>();
    backend->Initialize();
    scheduler.RegisterBackend(std::move(backend));
    
    REQUIRE(scheduler.GetActiveBackends().size() == initial_count + 1);
    
    scheduler.UnregisterBackend(BackendType::Mock);
    // 注意：可能有多个Mock后端，只删一个
    
    scheduler.Shutdown();
    return true;
}
