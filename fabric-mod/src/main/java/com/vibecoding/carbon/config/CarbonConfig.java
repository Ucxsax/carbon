package com.vibecoding.carbon.config;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import net.fabricmc.loader.api.FabricLoader;
import java.io.*;

public class CarbonConfig {
    // Global toggle
    public boolean enableNPUAcceleration = true;

    // Backend selection: "Auto", "CPU (ORT)", "OpenVINO", "DirectML", "Mock"
    public String preferredBackend = "Auto";

    // Scheduling
    public int maxConcurrentTasks = 2;
    public float npuHighUtilizationThreshold = 0.85f;
    public int maxConsecutiveFailures = 5;
    public int circuitBreakerCooldownMs = 5000;
    public boolean enableAutoDegrade = true;
    public boolean enableCircuitBreaker = true;

    // Backend toggles
    public boolean enableOpenVINO = true;
    public boolean enableTensorRT = true;
    public boolean enableDirectML = true;

    // Feature toggles
    public boolean enableChunkMeshAcceleration = true;
    public boolean enableLightBakingAcceleration = true;
    public boolean enableSSAOAcceleration = false;
    public boolean enableTextureUpscale = false;

    // HUD overlay
    public boolean showOverlay = true;

    private static final String CONFIG_FILE = "carbon.json";

    public static CarbonConfig load() {
        File configFile = new File(FabricLoader.getInstance().getConfigDir().toFile(), CONFIG_FILE);
        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        if (configFile.exists()) {
            try (FileReader reader = new FileReader(configFile)) {
                CarbonConfig loaded = gson.fromJson(reader, CarbonConfig.class);
                if (loaded != null) {
                    return loaded;
                }
            } catch (Exception e) {
                // fall through to defaults
            }
        }
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
            // silent
        }
    }
}
