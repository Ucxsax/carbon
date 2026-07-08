cmake_minimum_required(VERSION 3.20)
project(Carbon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 选项
option(CARBON_BUILD_TESTS "Build tests" ON)
option(CARBON_USE_OPENVINO "Enable Intel OpenVINO backend" OFF)
option(CARBON_USE_TENSORRT "Enable NVIDIA TensorRT backend" OFF)
option(CARBON_USE_DIRECTML "Enable DirectML backend" OFF)

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 源文件
set(CARBON_SOURCES
    src/scheduler/task_queue.cpp
    src/scheduler/npu_scheduler.cpp
    src/hal/npu_backend.cpp
    src/backends/mock_backend.cpp
    src/utils/logger.cpp
)

# 核心库
add_library(carbon STATIC ${CARBON_SOURCES})
target_include_directories(carbon PUBLIC include)

# 平台特定配置
if(WIN32)
    target_compile_definitions(carbon PRIVATE CARBON_PLATFORM_WINDOWS)
elseif(UNIX)
    target_compile_definitions(carbon PRIVATE CARBON_PLATFORM_LINUX)
endif()

# 测试
if(CARBON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
