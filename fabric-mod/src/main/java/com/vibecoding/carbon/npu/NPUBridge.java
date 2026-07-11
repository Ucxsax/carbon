package com.vibecoding.carbon.npu;

import com.vibecoding.carbon.CarbonMod;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.BiConsumer;

/**
 * NPU 原生库桥接层
 * 通过 JNI 调用 C++ Carbon 核心库
 */
public class NPUBridge {
    private boolean available = false;
    private boolean initialized = false;

    // 任务完成回调注册表
    private final Map<Long, BiConsumer<Boolean, Long>> taskCallbacks = new ConcurrentHashMap<>();

    static {
        try {
            System.loadLibrary("carbon_npu");
        } catch (UnsatisfiedLinkError e) {
            CarbonMod.LOGGER.warn("[Carbon] 无法加载 NPU 原生库: {}", e.getMessage());
        }
    }

    public NPUBridge() {
        try {
            available = isNativeAvailable();
        } catch (UnsatisfiedLinkError e) {
            available = false;
        }
    }

    public boolean initialize() {
        if (!available) return false;
        if (initialized) return true;

        initialized = nativeInitialize(
            CarbonMod.getConfig().npuHighUtilizationThreshold,
            CarbonMod.getConfig().maxConsecutiveFailures,
            CarbonMod.getConfig().circuitBreakerCooldownMs,
            CarbonMod.getConfig().enableAutoDegrade,
            CarbonMod.getConfig().enableCircuitBreaker
        );
        return initialized;
    }

    public void shutdown() {
        if (initialized) {
            nativeShutdown();
            initialized = false;
            taskCallbacks.clear();
        }
    }

    public boolean isAvailable() {
        return available && initialized;
    }

    public List<String> getActiveBackends() {
        if (!initialized) return new ArrayList<>();
        return nativeGetActiveBackends();
    }

    public float getAverageUtilization() {
        if (!initialized) return 0.0f;
        return nativeGetAverageUtilization();
    }

    public int getPendingTaskCount() {
        if (!initialized) return 0;
        return nativeGetPendingTaskCount();
    }

    /**
     * 提交区块网格生成任务
     */
    public long submitChunkMeshTask(int chunkX, int chunkZ, int[] blockData,
                                     BiConsumer<Boolean, Long> callback) {
        if (!initialized || !CarbonMod.getConfig().enableChunkMeshAcceleration) {
            return 0;
        }
        long taskId = nativeSubmitChunkMeshTask(chunkX, chunkZ, blockData);
        if (taskId > 0 && callback != null) {
            taskCallbacks.put(taskId, callback);
        }
        return taskId;
    }

    /**
     * 提交光照烘焙任务
     */
    public long submitLightBakingTask(int chunkX, int chunkZ, byte[] skyLight, byte[] blockLight,
                                       BiConsumer<Boolean, Long> callback) {
        if (!initialized || !CarbonMod.getConfig().enableLightBakingAcceleration) {
            return 0;
        }
        long taskId = nativeSubmitLightBakingTask(chunkX, chunkZ, skyLight, blockLight);
        if (taskId > 0 && callback != null) {
            taskCallbacks.put(taskId, callback);
        }
        return taskId;
    }

    /**
     * JNI 回调入口 - 通用任务完成
     */
    public void onTaskComplete(long taskId, long executionTimeUs, boolean success, String errorMessage) {
        BiConsumer<Boolean, Long> callback = taskCallbacks.remove(taskId);
        if (callback != null) {
            try {
                callback.accept(success, executionTimeUs);
            } catch (Exception e) {
                CarbonMod.LOGGER.error("[Carbon] 任务回调异常: taskId={}", taskId, e);
            }
        }

        if (!success) {
            CarbonMod.LOGGER.warn("[Carbon] NPU 任务失败: taskId={}, error={}", taskId, errorMessage);
        }
    }

    /**
     * JNI 回调入口 - 区块网格生成完成
     */
    public void onChunkMeshComplete(long taskId, long executionTimeUs, boolean success, String errorMessage) {
        BiConsumer<Boolean, Long> callback = taskCallbacks.remove(taskId);
        if (callback != null) {
            try {
                callback.accept(success, executionTimeUs);
            } catch (Exception e) {
                CarbonMod.LOGGER.error("[Carbon] 区块网格任务回调异常: taskId={}", taskId, e);
            }
        }

        if (success) {
            CarbonMod.LOGGER.debug("[Carbon] 区块网格 NPU 计算完成: taskId={}, 耗时={}us", taskId, executionTimeUs);
        } else {
            CarbonMod.LOGGER.warn("[Carbon] 区块网格任务失败: taskId={}, error={}", taskId, errorMessage);
        }
    }

    /**
     * JNI 回调入口 - 光照烘焙完成
     */
    public void onLightBakeComplete(long taskId, long executionTimeUs, boolean success, String errorMessage) {
        BiConsumer<Boolean, Long> callback = taskCallbacks.remove(taskId);
        if (callback != null) {
            try {
                callback.accept(success, executionTimeUs);
            } catch (Exception e) {
                CarbonMod.LOGGER.error("[Carbon] 光照烘焙任务回调异常: taskId={}", taskId, e);
            }
        }

        if (success) {
            CarbonMod.LOGGER.debug("[Carbon] 光照烘焙 NPU 计算完成: taskId={}, 耗时={}us", taskId, executionTimeUs);
        } else {
            CarbonMod.LOGGER.warn("[Carbon] 光照烘焙任务失败: taskId={}, error={}", taskId, errorMessage);
        }
    }

    // ========== 原生方法 ==========

    private native boolean isNativeAvailable();

    private native boolean nativeInitialize(
        float utilizationThreshold,
        int maxFailures,
        int cooldownMs,
        boolean autoDegrade,
        boolean circuitBreaker
    );

    private native void nativeShutdown();

    private native List<String> nativeGetActiveBackends();

    private native float nativeGetAverageUtilization();

    private native int nativeGetPendingTaskCount();

    private native long nativeSubmitChunkMeshTask(int chunkX, int chunkZ, int[] blockData);

    private native long nativeSubmitLightBakingTask(int chunkX, int chunkZ, byte[] skyLight, byte[] blockLight);
}
