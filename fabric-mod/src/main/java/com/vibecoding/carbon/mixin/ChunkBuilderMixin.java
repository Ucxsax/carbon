package com.vibecoding.carbon.mixin;

import com.vibecoding.carbon.CarbonMod;
import net.minecraft.client.render.chunk.ChunkBuilder;
import net.minecraft.client.render.chunk.ChunkRendererRegion;
import net.minecraft.util.math.BlockPos;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

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

        if (!CarbonMod.getConfig().enableChunkMeshAcceleration) {
            return;
        }

        try {
            ChunkBuilder.BuiltChunk thiz = (ChunkBuilder.BuiltChunk) (Object) this;
            BlockPos origin = thiz.getOrigin();
            int chunkX = origin.getX() >> 4;
            int chunkZ = origin.getZ() >> 4;

            CarbonMod.LOGGER.debug("[Carbon] Chunk rebuild detected: ({}, {})", chunkX, chunkZ);
        } catch (Exception e) {
            CarbonMod.LOGGER.warn("[Carbon] Chunk rebuild hook failed: {}", e.getMessage());
        }
    }
}
