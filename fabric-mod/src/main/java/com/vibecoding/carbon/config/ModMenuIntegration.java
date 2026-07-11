package com.vibecoding.carbon.config;

import com.terraformersmc.modmenu.api.ConfigScreenFactory;
import com.terraformersmc.modmenu.api.ModMenuApi;
import com.vibecoding.carbon.CarbonMod;
import me.shedaniel.clothconfig2.api.ConfigBuilder;
import me.shedaniel.clothconfig2.api.ConfigCategory;
import me.shedaniel.clothconfig2.api.ConfigEntryBuilder;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.text.Text;

/**
 * Mod Menu 配置界面集成
 * 基于 Cloth Config 构建可视化配置面板
 */
public class ModMenuIntegration implements ModMenuApi {

    @Override
    public ConfigScreenFactory<?> getModConfigScreenFactory() {
        return this::buildConfigScreen;
    }

    private Screen buildConfigScreen(Screen parent) {
        CarbonConfig config = CarbonMod.getConfig();
        CarbonConfig defaults = new CarbonConfig();

        ConfigBuilder builder = ConfigBuilder.create()
            .setParentScreen(parent)
            .setTitle(Text.literal("Carbon NPU 设置"))
            .setSavingRunnable(config::save);

        ConfigEntryBuilder entryBuilder = builder.entryBuilder();

        // ========== 通用设置 ==========
        ConfigCategory general = builder.getOrCreateCategory(Text.literal("通用设置"));

        general.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("启用 NPU 加速"),
            config.enableNPUAcceleration
        ).setDefaultValue(defaults.enableNPUAcceleration)
            .setSaveConsumer(val -> config.enableNPUAcceleration = val)
            .setTooltip(Text.literal("全局总开关，关闭后所有 NPU 加速功能禁用"))
            .build());

        general.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("启用自动降级"),
            config.enableAutoDegrade
        ).setDefaultValue(defaults.enableAutoDegrade)
            .setSaveConsumer(val -> config.enableAutoDegrade = val)
            .setTooltip(Text.literal("NPU 占用过高时自动降级到 GPU/CPU 计算"))
            .build());

        general.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("启用熔断机制"),
            config.enableCircuitBreaker
        ).setDefaultValue(defaults.enableCircuitBreaker)
            .setSaveConsumer(val -> config.enableCircuitBreaker = val)
            .setTooltip(Text.literal("连续失败时自动熔断，冷却后恢复，防止崩溃"))
            .build());

        // ========== 阈值设置 ==========
        ConfigCategory thresholds = builder.getOrCreateCategory(Text.literal("阈值设置"));

        thresholds.addEntry(entryBuilder.startFloatField(
            Text.literal("NPU 高占用阈值"),
            config.npuHighUtilizationThreshold
        ).setDefaultValue(defaults.npuHighUtilizationThreshold)
            .setMin(0.1f).setMax(1.0f)
            .setSaveConsumer(val -> config.npuHighUtilizationThreshold = val)
            .setTooltip(Text.literal("超过此占用率时触发自动降级 (默认 0.85 = 85%)"))
            .build());

        thresholds.addEntry(entryBuilder.startIntField(
            Text.literal("最大连续失败次数"),
            config.maxConsecutiveFailures
        ).setDefaultValue(defaults.maxConsecutiveFailures)
            .setMin(1).setMax(100)
            .setSaveConsumer(val -> config.maxConsecutiveFailures = val)
            .setTooltip(Text.literal("连续失败多少次触发熔断 (默认 5)"))
            .build());

        thresholds.addEntry(entryBuilder.startIntField(
            Text.literal("熔断冷却时间 (ms)"),
            config.circuitBreakerCooldownMs
        ).setDefaultValue(defaults.circuitBreakerCooldownMs)
            .setMin(1000).setMax(600000)
            .setSaveConsumer(val -> config.circuitBreakerCooldownMs = val)
            .setTooltip(Text.literal("熔断后多久恢复 (默认 5000ms)"))
            .build());

        // ========== 功能开关 ==========
        ConfigCategory features = builder.getOrCreateCategory(Text.literal("功能开关"));

        features.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("区块网格生成加速 (P0)"),
            config.enableChunkMeshAcceleration
        ).setDefaultValue(defaults.enableChunkMeshAcceleration)
            .setSaveConsumer(val -> config.enableChunkMeshAcceleration = val)
            .setTooltip(Text.literal("将区块顶点网格计算分流到 NPU，减轻 CPU 单线程压力"))
            .build());

        features.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("光照烘焙加速 (P0)"),
            config.enableLightBakingAcceleration
        ).setDefaultValue(defaults.enableLightBakingAcceleration)
            .setSaveConsumer(val -> config.enableLightBakingAcceleration = val)
            .setTooltip(Text.literal("并行预计算光照烘焙数据"))
            .build());

        features.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("SSAO 预处理加速 (P1)"),
            config.enableSSAOAcceleration
        ).setDefaultValue(defaults.enableSSAOAcceleration)
            .setSaveConsumer(val -> config.enableSSAOAcceleration = val)
            .setTooltip(Text.literal("屏幕空间环境光遮蔽预计算 (开发中)"))
            .build());

        features.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("AI 纹理超分 (P1)"),
            config.enableTextureUpscale
        ).setDefaultValue(defaults.enableTextureUpscale)
            .setSaveConsumer(val -> config.enableTextureUpscale = val)
            .setTooltip(Text.literal("AI 超分辨率优化低清纹理 (开发中)"))
            .build());

        // ========== 硬件后端 ==========
        ConfigCategory hardware = builder.getOrCreateCategory(Text.literal("硬件后端"));

        hardware.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("Intel OpenVINO (Ultra NPU)"),
            config.enableOpenVINO
        ).setDefaultValue(defaults.enableOpenVINO)
            .setSaveConsumer(val -> config.enableOpenVINO = val)
            .setTooltip(Text.literal("Intel 酷睿 Ultra 内置 NPU 后端 (开发中)"))
            .build());

        hardware.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("NVIDIA TensorRT"),
            config.enableTensorRT
        ).setDefaultValue(defaults.enableTensorRT)
            .setSaveConsumer(val -> config.enableTensorRT = val)
            .setTooltip(Text.literal("NVIDIA RTX Tensor Core 后端 (开发中)"))
            .build());

        hardware.addEntry(entryBuilder.startBooleanToggle(
            Text.literal("DirectML (AMD/通用)"),
            config.enableDirectML
        ).setDefaultValue(defaults.enableDirectML)
            .setSaveConsumer(val -> config.enableDirectML = val)
            .setTooltip(Text.literal("DirectML 通用后端，支持 AMD NPU 和所有 DirectX 12 GPU (开发中)"))
            .build());

        return builder.build();
    }
}
