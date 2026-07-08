package com.vibecoding.carbon;

import com.vibecoding.carbon.config.CarbonConfig;
import com.vibecoding.carbon.npu.NPUBridge;
import net.fabricmc.api.ClientModInitializer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class CarbonMod implements ClientModInitializer {
    public static final String MOD_ID = "carbon";
    public static final Logger LOGGER = LoggerFactory.getLogger(MOD_ID);

    private static CarbonConfig config;
    private static NPUBridge npuBridge;

    @Override
    public void onInitializeClient() {
        LOGGER.info("[Carbon] 正在初始化 NPU 加速模组...");

        // 加载配置
        config = CarbonConfig.load();

        // 初始化 NPU 桥接层
        if (config.enableNPUAcceleration) {
            npuBridge = new NPUBridge();
            if (npuBridge.initialize()) {
                LOGGER.info("[Carbon] NPU 加速引擎初始化成功");
                LOGGER.info("[Carbon] 活跃后端: {}", npuBridge.getActiveBackends());
            } else {
                LOGGER.warn("[Carbon] NPU 加速引擎初始化失败，将使用原生 CPU 计算");
            }
        } else {
            LOGGER.info("[Carbon] NPU 加速已在配置中关闭");
        }

        LOGGER.info("[Carbon] 模组初始化完成");
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
