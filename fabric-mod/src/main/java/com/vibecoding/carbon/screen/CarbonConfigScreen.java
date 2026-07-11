package com.vibecoding.carbon.screen;

import com.vibecoding.carbon.CarbonMod;
import com.vibecoding.carbon.config.CarbonConfig;
import me.shedaniel.clothconfig2.api.ConfigBuilder;
import me.shedaniel.clothconfig2.api.ConfigCategory;
import me.shedaniel.clothconfig2.api.ConfigEntryBuilder;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.text.Text;

public class CarbonConfigScreen {

    public static Screen build(Screen parent) {
        CarbonConfig config = CarbonMod.getConfig();
        CarbonConfig defaults = new CarbonConfig();

        ConfigBuilder builder = ConfigBuilder.create()
            .setParentScreen(parent)
            .setTitle(Text.literal("Carbon NPU \u8BBE\u7F6E"))
            .setSavingRunnable(() -> {
                config.save();
                // Restart engine if needed
                CarbonMod.restartEngine();
            });

        ConfigEntryBuilder eb = builder.entryBuilder();

        // ========== General ==========
        ConfigCategory general = builder.getOrCreateCategory(Text.literal("\u901A\u7528\u8BBE\u7F6E"));

        general.addEntry(eb.startBooleanToggle(
            Text.literal("\u542F\u7528 NPU \u52A0\u901F"),
            config.enableNPUAcceleration
        ).setDefaultValue(defaults.enableNPUAcceleration)
            .setSaveConsumer(val -> config.enableNPUAcceleration = val)
            .setTooltip(Text.literal("\u5168\u5C40\u603B\u5F00\u5173\uFF0C\u5173\u95ED\u540E\u6240\u6709 NPU \u52A0\u901F\u529F\u80FD\u7981\u7528"))
            .build());

        general.addEntry(eb.startBooleanToggle(
            Text.literal("HUD \u663E\u793A"),
            config.showOverlay
        ).setDefaultValue(defaults.showOverlay)
            .setSaveConsumer(val -> config.showOverlay = val)
            .setTooltip(Text.literal("\u5728\u5C4F\u5E55\u5DE6\u4E0A\u89D2\u663E\u793A Carbon \u72B6\u6001\u4FE1\u606F"))
            .build());

        // ========== Backend ==========
        ConfigCategory backend = builder.getOrCreateCategory(Text.literal("\u786C\u4EF6\u540E\u7AEF"));

        String[] backendOptions = {"Auto", "CPU (ORT)", "Mock"};
        general.addEntry(eb.startSelector(
            Text.literal("\u8BA1\u7B97\u540E\u7AEF"),
            backendOptions,
            config.preferredBackend
        ).setDefaultValue(defaults.preferredBackend)
            .setSaveConsumer(val -> config.preferredBackend = val)
            .setTooltip(Text.literal("Auto: \u81EA\u52A8\u68C0\u6D4B\u6700\u4F18\u540E\u7AEF"),
                Text.literal("CPU (ORT): \u4F7F\u7528 ONNX Runtime CPU \u63A8\u7406"),
                Text.literal("Mock: \u6A21\u62DF\u540E\u7AEF\uFF0C\u4EC5\u7528\u4E8E\u6D4B\u8BD5"))
            .build());

        backend.addEntry(eb.startIntField(
            Text.literal("\u6700\u5927\u5E76\u884C\u4EFB\u52A1\u6570"),
            config.maxConcurrentTasks
        ).setDefaultValue(defaults.maxConcurrentTasks)
            .setMin(1).setMax(16)
            .setSaveConsumer(val -> config.maxConcurrentTasks = val)
            .setTooltip(Text.literal("\u540C\u65F6\u5904\u7406\u7684 NPU \u4EFB\u52A1\u6570\u91CF"))
            .build());

        backend.addEntry(eb.startBooleanToggle(
            Text.literal("\u542F\u7528\u81EA\u52A8\u964D\u7EA7"),
            config.enableAutoDegrade
        ).setDefaultValue(defaults.enableAutoDegrade)
            .setSaveConsumer(val -> config.enableAutoDegrade = val)
            .setTooltip(Text.literal("NPU \u5360\u7528\u8FC7\u9AD8\u65F6\u81EA\u52A8\u964D\u7EA7\u5230 CPU \u8BA1\u7B97"))
            .build());

        backend.addEntry(eb.startBooleanToggle(
            Text.literal("\u542F\u7528\u7194\u65AD\u673A\u5236"),
            config.enableCircuitBreaker
        ).setDefaultValue(defaults.enableCircuitBreaker)
            .setSaveConsumer(val -> config.enableCircuitBreaker = val)
            .setTooltip(Text.literal("\u8FDE\u7EED\u5931\u8D25\u65F6\u81EA\u52A8\u7194\u65AD\uFF0C\u51B7\u5374\u540E\u6062\u590D"))
            .build());

        backend.addEntry(eb.startFloatField(
            Text.literal("\u964D\u7EA7\u89E6\u53D1\u9608\u503C"),
            config.npuHighUtilizationThreshold
        ).setDefaultValue(defaults.npuHighUtilizationThreshold)
            .setMin(0.1f).setMax(1.0f)
            .setSaveConsumer(val -> config.npuHighUtilizationThreshold = val)
            .setTooltip(Text.literal("NPU \u5360\u7528\u7387\u8D85\u8FC7\u6B64\u503C\u65F6\u89E6\u53D1\u81EA\u52A8\u964D\u7EA7 (0.85 = 85%)"))
            .build());

        backend.addEntry(eb.startIntField(
            Text.literal("\u6700\u5927\u8FDE\u7EED\u5931\u8D25\u6B21\u6570"),
            config.maxConsecutiveFailures
        ).setDefaultValue(defaults.maxConsecutiveFailures)
            .setMin(1).setMax(100)
            .setSaveConsumer(val -> config.maxConsecutiveFailures = val)
            .setTooltip(Text.literal("\u8FDE\u7EED\u5931\u8D25\u591A\u5C11\u6B21\u89E6\u53D1\u7194\u65AD (5)"))
            .build());

        backend.addEntry(eb.startIntField(
            Text.literal("\u7194\u65AD\u51B7\u5374\u65F6\u95F4 (ms)"),
            config.circuitBreakerCooldownMs
        ).setDefaultValue(defaults.circuitBreakerCooldownMs)
            .setMin(1000).setMax(600000)
            .setSaveConsumer(val -> config.circuitBreakerCooldownMs = val)
            .setTooltip(Text.literal("\u7194\u65AD\u540E\u591A\u4E45\u6062\u590D (5000ms)"))
            .build());

        // ========== Features ==========
        ConfigCategory features = builder.getOrCreateCategory(Text.literal("\u529F\u80FD\u5F00\u5173"));

        features.addEntry(eb.startBooleanToggle(
            Text.literal("\u533A\u5757\u7F51\u683C\u751F\u6210\u52A0\u901F (P0)"),
            config.enableChunkMeshAcceleration
        ).setDefaultValue(defaults.enableChunkMeshAcceleration)
            .setSaveConsumer(val -> config.enableChunkMeshAcceleration = val)
            .setTooltip(Text.literal("\u5C06\u533A\u5757\u9876\u70B9\u7F51\u683C\u8BA1\u7B97\u5206\u6D41\u5230 NPU"))
            .build());

        features.addEntry(eb.startBooleanToggle(
            Text.literal("\u5149\u7167\u70D8\u7119\u52A0\u901F (P0)"),
            config.enableLightBakingAcceleration
        ).setDefaultValue(defaults.enableLightBakingAcceleration)
            .setSaveConsumer(val -> config.enableLightBakingAcceleration = val)
            .setTooltip(Text.literal("\u5E76\u884C\u9884\u8BA1\u7B97\u5149\u7167\u70D8\u7119\u6570\u636E"))
            .build());

        features.addEntry(eb.startBooleanToggle(
            Text.literal("SSAO \u9884\u5904\u7406\u52A0\u901F (P1)"),
            config.enableSSAOAcceleration
        ).setDefaultValue(defaults.enableSSAOAcceleration)
            .setSaveConsumer(val -> config.enableSSAOAcceleration = val)
            .setTooltip(Text.literal("\u5C4F\u5E55\u7A7A\u95F4\u73AF\u5149\u906E\u853D\u9884\u8BA1\u7B97 (\u5F00\u53D1\u4E2D)"))
            .build());

        features.addEntry(eb.startBooleanToggle(
            Text.literal("AI \u7EB9\u7406\u8D85\u5206 (P1)"),
            config.enableTextureUpscale
        ).setDefaultValue(defaults.enableTextureUpscale)
            .setSaveConsumer(val -> config.enableTextureUpscale = val)
            .setTooltip(Text.literal("AI \u8D85\u5206\u8FA8\u7387\u4F18\u5316\u4F4E\u6E05\u7EB9\u7406 (\u5F00\u53D1\u4E2D)"))
            .build());

        return builder.build();
    }
}
