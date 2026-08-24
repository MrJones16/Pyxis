#pragma once

#include <Renderer/UI/UIModule.h>

namespace Pyxis {

struct UIComponent {
    bool m_Enabled = true;
    Ref<UIModule> m_RootModule;

    UIComponent(Ref<UIModule> module) : m_RootModule(module) {};
    inline void Layout() const {
        if (!m_Enabled)
            return;
        m_RootModule->DrawUI();
    };
};

} // namespace Pyxis
