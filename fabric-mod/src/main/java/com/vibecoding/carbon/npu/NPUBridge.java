package com.vibecoding.carbon.npu;

import com.vibecoding.carbon.CarbonMod;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.BiConsumer;

/**
 * NPU 原生库桥接层
 * 通过 JNI 调用 C++ Carbon 核心库
 * 启动时自动从 JAR 释放 DLL 和 ONNX 模型到临时目录
 */
public class NPUBridge {
    private boolean available = false;
    private boolean initialized = false;
    private static Path nativeDir;

    // 任务完成回调注册表
    private final Map<Long, BiConsumer<Boolean, Long>> taskCallbacks = new ConcurrentHashMap<>();

    static {
        try {
            nativeDir = extractNativeLibs();
            if (nativeDir != null) {
                // 先加载 ORT runtime，再加载 carbon_npu
                Path ortDll = nativeDir.resolve("onnxruntime.dll");
                Path carbonDll = nativeDir.resolve("carbon_npu.dll");
                Path modelsDir = nativeDir;

                if (Files.exists(ortDll)) {
                    System.load(ortDll.toAbsolutePath().toString());
                    CarbonMod.LOGGER.info("[Carbon] 已加载 onnxruntime.dll");
                }
                if (Files.exists(carbonDll)) {
                    System.load(carbonDll.toAbsolutePath().toString());
                    CarbonMod.LOGGER.info("[Carbon] 已加载 carbon_npu.dll");
                }
            }
        } catch (Exception e) {
            CarbonMod.LOGGER.warn("[Carbon] 无法加载 NPU 原生库: {}", e.getMessage());
        }
    }

    /**
     * 从 JAR 内释放 native 库到游戏目录下的 carbon/native/
     */
    private static Path extractNativeLibs() {
        try {
            // 目标目录: .minecraft/carbon/native/
            Path gameDir = getGameDir();
            Path targetDir = gameDir.resolve("carbon").resolve("native");
            Files.createDirectories(targetDir);

            String[] files = {"carbon_npu.dll", "onnxruntime.dll", "chunk_mesh_v1.onnx", "light_baking_v1.onnx"};

            for (String fileName : files) {
                Path target = targetDir.resolve(fileName);
                String resourcePath = "/native/win-x64/" + fileName;

                InputStream is = NPUBridge.class.getResourceAsStream(resourcePath);
                if (is == null) {
                    CarbonMod.LOGGER.warn("[Carbon] JAR 中未找到资源: {}", resourcePath);
                    continue;
                }

                // 检查是否已释放且大小一致
                byte[] data = is.readAllBytes();
                is.close();

                if (Files.exists(target) && Files.size(target) == data.length) {
                    CarbonMod.LOGGER.debug("[Carbon] 资源已存在，跳过: {}", fileName);
                    continue;
                }

                Files.write(target, data);
                CarbonMod.LOGGER.info("[Carbon] 已释放: {} ({} bytes)", fileName, data.length);
            }

            return targetDir;
        } catch (Exception e) {
            CarbonMod.LOGGER.error("[Carbon] 释放 native 库失败", e);
            return null;
        }
    }

    private static Path getGameDir() {
        // Fabric 提供的游戏目录
        try {
            // 通过 net.fabricmc.loader 找到游戏目录
            Class<?> loaderClass = Class.forName("net.fabricmc.loader.api.FabricLoader");
            Object instance = loaderClass.getMethod("getInstance").invoke(null);
            java.nio.file.Path gameDir = (java.nio.file.Path) loaderClass.getMethod("getGameDir").invoke(instance);
            return gameDir;
        } catch (Exception e) {
            // 回退: 使用当前工作目录
            return Paths.get("").toAbsolutePath();
        }
    }

    public NPUBridge() {
        try {
            available = isNativeAvailable();
        } catch (UnsatisfiedLinkError e) {
            available = false;
        }
    }

    /**
     * 获取 native 库释放目录（供其他组件使用）
     */
    public static Path getNativeDir() {
        return nativeDir;
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
