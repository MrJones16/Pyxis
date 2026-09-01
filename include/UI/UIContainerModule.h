#pragma once
#include <Renderer/UI/UIModule.h>

namespace Pyxis {
struct UIContainerModule : UIModule {
    UIContainerModule(const std::string &id, Clay_ElementDeclaration config)
        : UIModule(id, config) {};

    virtual inline void DrawUI() override {
        if (!m_Enabled)
            return;

        CLAY(GetClayID(), m_Config) {
            for (auto &module : m_Children) {
                module->DrawUI();
            }
        }
    }
};
} // namespace Pyxis
