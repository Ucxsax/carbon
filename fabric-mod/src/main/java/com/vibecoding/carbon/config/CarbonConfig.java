package com.vibecoding.carbon.config;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import net.fabricmc.loader.api.FabricLoader;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

public class CarbonConfig {
    // 全局开关
    public boolean enableNPUAcceleration = true;

    // 后端开关
    public boolean enableOpenVINO = true;
    public boolean enableTensorRT = true;
    public boolean enableDirectML = true;

    // 调度配置
    public float npuHighUtilizationThreshold = 0.85f;
    public int maxConsecutiveFailures = 5;
    public int circuitBreakerCooldownMs = 5000;
    public boolean enableAutoDegrade = true;
    public boolean enableCircuitBreaker = true;

    // 功能开关
    public boolean enableChunkMeshAcceleration = true;
    public boolean enableLightBakingAcceleration = true;
    public boolean enableSSAOAcceleration = false;
    public boolean enableTextureUpscale = false;

    private static final String CONFIG_FILE = "carbon.json";

    public static CarbonConfig load() {
        File configFile = new File(FabricLoader.getInstance().getConfigDir().toFile(), CONFIG_FILE);
        Gson gson = new GsonBuilder().setPrettyPrinting().create();

        if (configFile.exists()) {
            try (FileReader reader = new FileReader(configFile)) {
                return gson.fromJson(reader, CarbonConfig.class);
            } catch (IOException e) {
                // 读取失败，返回默认配置
            }
        }

        // 保存默认配置
        CarbonConfig config = new CarbonConfig();
        config.save();
        return config;
    }

    public void save() {
        File configFile = new File(FabricLoader.getInstance().getConfigDir().toFile(), CONFIG_FILE);
        Gson gson = new GsonBuilder().setPrettyPrinting().create();

        try (FileWriter writer = new FileWriter(configFile)) {
            gson.toJson(this, writer);
        } catch (IOException e) {
            // 忽略保存错误
        }
    }
}
