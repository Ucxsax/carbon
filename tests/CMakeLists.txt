# 简单测试 - 不依赖外部测试框架
add_executable(carbon_tests
    test_main.cpp
    test_task_queue.cpp
    test_mock_backend.cpp
    test_scheduler.cpp
)

target_link_libraries(carbon_tests PRIVATE carbon pthread)

add_test(NAME carbon_tests COMMAND carbon_tests)
