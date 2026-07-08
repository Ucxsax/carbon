package com.vibecoding.carbon.config;

import com.terraformersmc.modmenu.api.ConfigScreenFactory;
import com.terraformersmc.modmenu.api.ModMenuApi;

/**
 * Mod Menu 配置界面集成
 * （可选依赖，没有装 Mod Menu 不影响功能）
 */
public class ModMenuIntegration implements ModMenuApi {

    @Override
    public ConfigScreenFactory<?> getModConfigScreenFactory() {
        // TODO: 实现 Cloth Config 配置界面
        // 目前返回 null，后续接入 Cloth Config
        return screen -> null;
    }
}
