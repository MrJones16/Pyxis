#pragma once

#include <UI/UIModule.h>

namespace Pyxis {

struct UIComponent {
    bool m_Enabled = true;
    Ref<UIModule> m_RootModule = nullptr;

    UIComponent() {};
    UIComponent(Ref<UIModule> module) : m_RootModule(module) {};
    inline void Layout() const {
        if (!m_Enabled || m_RootModule == nullptr)
            return;
        m_RootModule->DrawUI();
    };
};

} // namespace Pyxis
