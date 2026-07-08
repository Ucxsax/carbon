package com.vibecoding.carbon.mixin;

import com.vibecoding.carbon.CarbonMod;
import com.vibecoding.carbon.npu.NPUBridge;
import net.minecraft.client.render.LightmapTextureManager;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

/**
 * 光照烘焙加速 Mixin
 * P0 优先级 - 光照并行预计算
 * 
 * 注入点：光照贴图更新时，将光照计算分流到 NPU
 * 
 * 测试版说明：目前只做框架验证，不修改实际光照结果。
 * 正式版将对接区块光照烘焙管线，做并行预计算。
 */
@Mixin(LightmapTextureManager.class)
public abstract class LightmapMixin {

    @Inject(
        method = "update",
        at = @At("HEAD"),
        cancellable = false
    )
    private void onUpdateLightmap(float delta, CallbackInfo ci) {
        if (!CarbonMod.isNPUAvailable()) {
            return;
        }

        if (CarbonMod.getConfig().enableLightBakingAcceleration) {
            NPUBridge bridge = CarbonMod.getNpuBridge();
            
            // 测试版：仅记录待处理任务数，验证调度器连通性
            int pending = bridge.getPendingTaskCount();
            float util = bridge.getAverageUtilization();
            
            CarbonMod.LOGGER.debug("[Carbon] 光照更新 - NPU占用: {}, 待处理: {}", util, pending);
            
            // TODO: 正式版 - 提取光照数据提交NPU预计算
            // 1. 从光照管理器提取天光/方块光数据
            // 2. 提交给 NPU 做并行烘焙
            // 3. 异步返回结果写回光照贴图
        }
    }
}
