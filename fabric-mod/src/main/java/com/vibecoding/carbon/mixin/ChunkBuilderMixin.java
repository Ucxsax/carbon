package com.vibecoding.carbon.mixin;

import com.vibecoding.carbon.CarbonMod;
import com.vibecoding.carbon.npu.NPUBridge;
import net.minecraft.block.BlockState;
import net.minecraft.client.render.chunk.ChunkBuilder;
import net.minecraft.client.render.chunk.ChunkRendererRegion;
import net.minecraft.util.math.BlockPos;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

/**
 * 区块网格生成加速 Mixin
 * P0 优先级 - 解决 MC 致命单线程瓶颈
 * 
 * 注入点：区块构建任务执行时，将顶点计算分流到 NPU
 * 
 * 测试版说明：目前只做数据采集和任务提交验证，
 * 实际渲染仍走原生流程，不会影响游戏正常运行。
 */
@Mixin(ChunkBuilder.BuiltChunk.class)
public abstract class ChunkBuilderMixin {

    @Inject(
        method = "rebuild",
        at = @At("HEAD"),
        cancellable = false
    )
    private void onRebuildChunk(
        ChunkRendererRegion region,
        CallbackInfoReturnable<Boolean> cir
    ) {
        if (!CarbonMod.isNPUAvailable()) {
            return;
        }

        NPUBridge bridge = CarbonMod.getNpuBridge();
        
        ChunkBuilder.BuiltChunk thiz = (ChunkBuilder.BuiltChunk) (Object) this;
        BlockPos origin = thiz.getOrigin();
        int chunkX = origin.getX() >> 4;
        int chunkZ = origin.getZ() >> 4;

        // 提取区块方块数据（16x256x16）
        int[] blockData = extractBlockData(region, origin);
        
        if (blockData != null) {
            long taskId = bridge.submitChunkMeshTask(chunkX, chunkZ, blockData, (success, execTimeUs) -> {
                if (success) {
                    CarbonMod.LOGGER.debug("[Carbon] 区块网格 NPU 计算完成: ({}, {}), 耗时: {}us", 
                        chunkX, chunkZ, execTimeUs);
                    // TODO: 正式版 - 将 NPU 计算的顶点数据写回区块缓冲
                }
            });
            
            if (taskId > 0) {
                CarbonMod.LOGGER.debug("[Carbon] 提交区块网格任务: ({}, {}), taskId={}", 
                    chunkX, chunkZ, taskId);
            }
        }
    }

    /**
     * 从 ChunkRendererRegion 提取 16x256x16 方块状态数据
     */
    private int[] extractBlockData(ChunkRendererRegion region, BlockPos origin) {
        if (region == null) return null;
        
        int sizeX = 16;
        int sizeY = 256;
        int sizeZ = 16;
        int[] data = new int[sizeX * sizeY * sizeZ];
        
        try {
            int index = 0;
            for (int y = 0; y < sizeY; y++) {
                for (int z = 0; z < sizeZ; z++) {
                    for (int x = 0; x < sizeX; x++) {
                        BlockPos pos = origin.add(x, y, z);
                        BlockState state = region.getBlockState(pos);
                        data[index++] = state.getBlock().hashCode();
                    }
                }
            }
            return data;
        } catch (Exception e) {
            CarbonMod.LOGGER.warn("[Carbon] 提取方块数据失败: {}", e.getMessage());
            return null;
        }
    }
}
