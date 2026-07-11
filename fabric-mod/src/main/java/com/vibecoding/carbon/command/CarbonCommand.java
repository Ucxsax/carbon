package com.vibecoding.carbon.command;

import com.mojang.brigadier.CommandDispatcher;
import com.mojang.brigadier.context.CommandContext;
import com.vibecoding.carbon.CarbonMod;
import com.vibecoding.carbon.npu.NPUBridge;
import net.fabricmc.fabric.api.client.command.v2.ClientCommandManager;
import net.fabricmc.fabric.api.client.command.v2.FabricClientCommandSource;
import net.minecraft.text.Text;

import java.util.List;

/**
 * Carbon 客户端调试命令
 * 用法: /carbon status
 */
public class CarbonCommand {

    public static void register(CommandDispatcher<FabricClientCommandSource> dispatcher) {
        dispatcher.register(
            ClientCommandManager.literal("carbon")
                .then(ClientCommandManager.literal("status")
                    .executes(CarbonCommand::printStatus))
                .then(ClientCommandManager.literal("reload")
                    .executes(CarbonCommand::reloadConfig))
                .executes(CarbonCommand::printStatus)
        );
    }

    private static int printStatus(CommandContext<FabricClientCommandSource> ctx) {
        NPUBridge bridge = CarbonMod.getNpuBridge();
        boolean available = bridge.isAvailable();
        float utilization = bridge.getAverageUtilization();
        int pending = bridge.getPendingTaskCount();
        List<String> backends = bridge.getActiveBackends();

        ctx.getSource().sendFeedback(Text.literal("=== Carbon NPU 状态 ==="));
        ctx.getSource().sendFeedback(Text.literal("NPU 加速: " + (available ? "§a已启用" : "§c未启用")));
        
        if (available) {
            ctx.getSource().sendFeedback(Text.literal("活跃后端: " + String.join(", ", backends)));
            ctx.getSource().sendFeedback(Text.literal(String.format("平均占用: %.1f%%", utilization * 100)));
            ctx.getSource().sendFeedback(Text.literal("待处理任务: " + pending));
            ctx.getSource().sendFeedback(Text.literal("区块网格加速: " + 
                (CarbonMod.getConfig().enableChunkMeshAcceleration ? "§a开" : "§7关")));
            ctx.getSource().sendFeedback(Text.literal("光照烘焙加速: " + 
                (CarbonMod.getConfig().enableLightBakingAcceleration ? "§a开" : "§7关")));
        } else {
            ctx.getSource().sendFeedback(Text.literal("§7原生库未加载，以兼容模式运行"));
        }

        return 1;
    }

    private static int reloadConfig(CommandContext<FabricClientCommandSource> ctx) {
        CarbonMod.getConfig().load();
        ctx.getSource().sendFeedback(Text.literal("§a配置已重载"));
        return 1;
    }
}
