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
        LOGGER.info("[Carbon] Carbon NPU Acceleration v0.2.0");
        LOGGER.info("========================================");

        config = CarbonConfig.load();
        LOGGER.info("[Carbon] Config loaded, backend={}", config.preferredBackend);

        npuBridge = new NPUBridge();
        if (config.enableNPUAcceleration) {
            if (npuBridge.initialize()) {
                LOGGER.info("[Carbon] NPU engine initialized, backends={}", npuBridge.getActiveBackends());
            } else {
                LOGGER.warn("[Carbon] NPU engine init failed, running native CPU");
            }
        } else {
            LOGGER.info("[Carbon] NPU acceleration disabled in config");
        }

        ClientCommandRegistrationCallback.EVENT.register((dispatcher, registryAccess) -> {
            CarbonCommand.register(dispatcher);
        });

        LOGGER.info("[Carbon] Mod initialized");
    }

    /**
     * Called from config screen save — restart engine with new settings
     */
    public static void restartEngine() {
        if (npuBridge != null) {
            npuBridge.shutdown();
        }
        config = CarbonConfig.load();
        if (config.enableNPUAcceleration) {
            npuBridge = new NPUBridge();
            if (npuBridge.initialize()) {
                LOGGER.info("[Carbon] Engine restarted, backends={}", npuBridge.getActiveBackends());
            } else {
                LOGGER.warn("[Carbon] Engine restart failed");
            }
        } else {
            npuBridge = new NPUBridge();
            LOGGER.info("[Carbon] NPU acceleration disabled");
        }
    }

    public static CarbonConfig getConfig() { return config; }
    public static NPUBridge getNpuBridge() { return npuBridge; }
    public static boolean isNPUAvailable() { return npuBridge != null && npuBridge.isAvailable(); }
}
