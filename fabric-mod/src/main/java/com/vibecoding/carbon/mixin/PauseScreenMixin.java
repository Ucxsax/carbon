package com.vibecoding.carbon.mixin;

import com.vibecoding.carbon.screen.CarbonConfigScreen;
import net.minecraft.client.gui.screen.GameMenuScreen;
import net.minecraft.client.gui.screen.Screen;
import net.minecraft.client.gui.widget.ButtonWidget;
import net.minecraft.text.Text;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfo;

import java.util.concurrent.atomic.AtomicBoolean;

@Mixin(GameMenuScreen.class)
public abstract class PauseScreenMixin extends Screen {

    private PauseScreenMixin(Text title) {
        super(title);
    }

    @Inject(method = "initWidgets", at = @At("RETURN"))
    private void addCarbonButton(CallbackInfo ci) {
        // Add full-width "Carbon配置" button after "回到游戏"
        // Position it at y = first button row + 27 (below "回到游戏")
        int centerX = this.width / 2;
        int buttonWidth = 204;
        int startX = centerX - buttonWidth / 2;

        // Find the y position: after "回到游戏" button (the first full-width button)
        // The first button starts at height/2 - 101 in vanilla, full-width row
        int firstButtonY = this.height / 2 - 101 + 24 + 5; // below "回到游戏"

        this.addDrawableChild(ButtonWidget.builder(
            Text.literal("Carbon \u914D\u7F6E"),
            button -> {
                this.client.setScreen(CarbonConfigScreen.build(this));
            }
        ).dimensions(startX, firstButtonY, buttonWidth, 20).build());
    }
}
