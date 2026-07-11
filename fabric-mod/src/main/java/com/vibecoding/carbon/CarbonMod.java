package com.vibecoding.carbon;

import com.vibecoding.carbon.command.CarbonCommand;
import com.vibecoding.carbon.config.CarbonConfig;
import com.vibecoding.carbon.npu.NPUBridge;
import net.fabricmc.api.ClientModInitializer;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandRegistrationCallback;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class CarbonMod implements ClientModInitializer {
    public static final String MOD_ID = "carbon";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

    private static CarbonConfig config;
    private static NPUBridge npuBridge;

    @Override
    public void onInitializeClient() {
        LOGGER.info("========================================");
        LOGGER.info("[Carbon] Carbon NPU 加速模组 v0.1.0 Alpha");
        LOGGER.info("========================================");

        // 加载配置
        config = CarbonConfig.load();
        LOGGER.info("[Carbon] 配置文件已加载");

        // 初始化 NPU 桥接层
        if (config.enableNPUAcceleration) {
            npuBridge = new NPUBridge();
            if (npuBridge.initialize()) {
                LOGGER.info("[Carbon] NPU 加速引擎初始化成功");
                LOGGER.info("[Carbon] 活跃后端: {}", npuBridge.getActiveBackends());
                LOGGER.info("[Carbon] 平均占用率: {}%", String.format("%.1f", npuBridge.getAverageUtilization() * 100));
            } else {
                LOGGER.warn("[Carbon] NPU 加速引擎初始化失败，降级为原生 CPU 计算");
                LOGGER.warn("[Carbon] 请确保 carbon_npu.dll 位于系统库路径或模组目录下");
            }
        } else {
            LOGGER.info("[Carbon] NPU 加速已在配置中关闭");
            npuBridge = new NPUBridge(); // 保留实例供命令查询
        }

        // 注册客户端命令
        ClientCommandRegistrationCallback.EVENT.register((dispatcher, registryAccess) -> {
            CarbonCommand.register(dispatcher);
            LOGGER.debug("[Carbon] 客户端命令已注册: /carbon");
        });

        LOGGER.info("[Carbon] 模组初始化完成");
        LOGGER.info("[Carbon] 输入 /carbon status 查看 NPU 状态");
    }

    public static CarbonConfig getConfig() {
        return config;
    }

    public static NPUBridge getNpuBridge() {
        return npuBridge;
    }

    public static boolean isNPUAvailable() {
        return npuBridge != null && npuBridge.isAvailable();
    }
}
