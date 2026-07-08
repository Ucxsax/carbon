#include <jni.h>
#include <string>
#include <vector>
#include <mutex>
#include "carbon/scheduler/npu_scheduler.h"
#include "carbon/common_types.h"

using namespace carbon;

// 全局 JVM 引用（用于回调时 AttachCurrentThread）
static JavaVM* g_jvm = nullptr;
static jobject g_bridge_obj = nullptr;  // NPUBridge 全局引用
static jmethodID g_onTaskComplete_mid = nullptr;

static std::mutex g_jni_mutex;

// JNI_OnLoad - 库加载时调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_17;
}

// 辅助：获取当前线程的 JNIEnv
static JNIEnv* GetJNIEnv() {
    JNIEnv* env = nullptr;
    if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_17) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

// 任务完成回调 - 从 C++ 调度器线程调用，转发给 Java
static void TaskCompleteCallback(uint64_t task_id, const TaskResult& result) {
    std::lock_guard<std::mutex> lock(g_jni_mutex);
    
    if (!g_bridge_obj || !g_onTaskComplete_mid) return;
    
    JNIEnv* env = GetJNIEnv();
    if (!env) return;
    
    jlong j_task_id = static_cast<jlong>(task_id);
    jboolean j_success = result.success ? JNI_TRUE : JNI_FALSE;
    jlong j_exec_time = static_cast<jlong>(result.execution_time_us);
    jstring j_error = result.success ? nullptr : env->NewStringUTF(result.error_message.c_str());
    
    env->CallVoidMethod(g_bridge_obj, g_onTaskComplete_mid, j_task_id, j_exec_time, j_success, j_error);
    
    if (j_error) {
        env->DeleteLocalRef(j_error);
    }
}

extern "C" {

JNIEXPORT jboolean JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_isNativeAvailable(
    JNIEnv* env, jobject obj
) {
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeInitialize(
    JNIEnv* env, jobject obj,
    jfloat utilizationThreshold,
    jint maxFailures,
    jint cooldownMs,
    jboolean autoDegrade,
    jboolean circuitBreaker
) {
    auto& scheduler = NPUScheduler::Instance();
    if (scheduler.IsInitialized()) {
        return JNI_TRUE;
    }
    
    // 保存 Java 对象全局引用和回调方法 ID
    if (g_bridge_obj == nullptr) {
        g_bridge_obj = env->NewGlobalRef(obj);
        jclass cls = env->GetObjectClass(obj);
        g_onTaskComplete_mid = env->GetMethodID(cls, "onTaskComplete", "(JJZLjava/lang/String;)V");
    }
    
    SchedulerConfig config;
    config.npu_high_utilization_threshold = utilizationThreshold;
    config.max_consecutive_failures = static_cast<size_t>(maxFailures);
    config.circuit_breaker_cooldown_ms = static_cast<uint64_t>(cooldownMs);
    config.enable_auto_degrade = autoDegrade;
    config.enable_circuit_breaker = circuitBreaker;
    
    return scheduler.Initialize(config) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeShutdown(
    JNIEnv* env, jobject obj
) {
    auto& scheduler = NPUScheduler::Instance();
    scheduler.Shutdown();
    
    std::lock_guard<std::mutex> lock(g_jni_mutex);
    if (g_bridge_obj) {
        env->DeleteGlobalRef(g_bridge_obj);
        g_bridge_obj = nullptr;
    }
    g_onTaskComplete_mid = nullptr;
}

JNIEXPORT jobject JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeGetActiveBackends(
    JNIEnv* env, jobject obj
) {
    auto& scheduler = NPUScheduler::Instance();
    auto backends = scheduler.GetActiveBackends();
    
    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    jmethodID initMethod = env->GetMethodID(arrayListClass, "<init>", "()V");
    jmethodID addMethod = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    
    jobject list = env->NewObject(arrayListClass, initMethod);
    
    for (auto type : backends) {
        std::string name;
        switch (type) {
            case BackendType::Mock: name = "Mock"; break;
            case BackendType::OpenVINO: name = "OpenVINO"; break;
            case BackendType::TensorRT: name = "TensorRT"; break;
            case BackendType::DirectML: name = "DirectML"; break;
            case BackendType::CPU_Fallback: name = "CPU_Fallback"; break;
            default: name = "Unknown"; break;
        }
        jstring jname = env->NewStringUTF(name.c_str());
        env->CallBooleanMethod(list, addMethod, jname);
        env->DeleteLocalRef(jname);
    }
    
    return list;
}

JNIEXPORT jfloat JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeGetAverageUtilization(
    JNIEnv* env, jobject obj
) {
    auto& scheduler = NPUScheduler::Instance();
    return scheduler.GetAverageUtilization();
}

JNIEXPORT jint JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeGetPendingTaskCount(
    JNIEnv* env, jobject obj
) {
    auto& scheduler = NPUScheduler::Instance();
    return static_cast<jint>(scheduler.GetPendingTaskCount());
}

JNIEXPORT jlong JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeSubmitChunkMeshTask(
    JNIEnv* env, jobject obj,
    jint chunkX, jint chunkZ,
    jintArray blockData
) {
    auto& scheduler = NPUScheduler::Instance();
    if (!scheduler.IsInitialized()) {
        return 0;
    }
    
    auto task = std::make_shared<NPUTask>();
    task->type = TaskType::ChunkMeshGen;
    task->priority = TaskPriority::Critical;
    task->model_name = "chunk_mesh_v1";
    
    jsize len = env->GetArrayLength(blockData);
    jint* data = env->GetIntArrayElements(blockData, nullptr);
    
    TensorDesc input;
    input.data_type = DataType::Int32;
    input.shape = {16, 256, 16};
    input.data = data;
    input.size_bytes = len * sizeof(jint);
    task->inputs.push_back(input);
    
    task->callback = TaskCompleteCallback;
    
    uint64_t task_id = scheduler.SubmitTask(task);
    
    env->ReleaseIntArrayElements(blockData, data, JNI_ABORT);
    
    return static_cast<jlong>(task_id);
}

JNIEXPORT jlong JNICALL Java_com_vibecoding_carbon_npu_NPUBridge_nativeSubmitLightBakingTask(
    JNIEnv* env, jobject obj,
    jint chunkX, jint chunkZ,
    jbyteArray skyLight, jbyteArray blockLight
) {
    auto& scheduler = NPUScheduler::Instance();
    if (!scheduler.IsInitialized()) {
        return 0;
    }
    
    auto task = std::make_shared<NPUTask>();
    task->type = TaskType::LightBaking;
    task->priority = TaskPriority::High;
    task->model_name = "light_baking_v1";
    
    jsize sky_len = env->GetArrayLength(skyLight);
    jbyte* sky_data = env->GetByteArrayElements(skyLight, nullptr);
    TensorDesc sky_input;
    sky_input.data_type = DataType::UInt8;
    sky_input.shape = {16, 256, 16};
    sky_input.data = sky_data;
    sky_input.size_bytes = sky_len;
    task->inputs.push_back(sky_input);
    
    jsize block_len = env->GetArrayLength(blockLight);
    jbyte* block_data = env->GetByteArrayElements(blockLight, nullptr);
    TensorDesc block_input;
    block_input.data_type = DataType::UInt8;
    block_input.shape = {16, 256, 16};
    block_input.data = block_data;
    block_input.size_bytes = block_len;
    task->inputs.push_back(block_input);
    
    task->callback = TaskCompleteCallback;
    
    uint64_t task_id = scheduler.SubmitTask(task);
    
    env->ReleaseByteArrayElements(skyLight, sky_data, JNI_ABORT);
    env->ReleaseByteArrayElements(blockLight, block_data, JNI_ABORT);
    
    return static_cast<jlong>(task_id);
}

} // extern "C"
