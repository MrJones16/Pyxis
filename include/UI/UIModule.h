#pragma once
#include <Core/Core.h>
#include <Renderer/clay.h>
#include <list>

namespace Pyxis {
struct UIModule {
    bool m_Enabled = true;
    Clay_ElementDeclaration m_Config;
    std::string m_ID;
    std::list<Ref<UIModule>> m_Children = {};
    // i don't think parents would be needed here,
    // as going up hierarchy is not needed

    UIModule(const std::string &id, Clay_ElementDeclaration config = {})
        : m_Config(config), m_ID(id) {};
    virtual ~UIModule() = default;

    void AddChildModule(Ref<UIModule> module) { m_Children.push_back(module); }
    void RemoveChildModule(Ref<UIModule> module) { m_Children.remove(module); }
    std::list<Ref<UIModule>> &GetChildren() { return m_Children; }

    // helper to clean up code
    inline Clay_ElementId GetClayID() const {
        return CLAY_SID(Clay_String(false, static_cast<int32_t>(m_ID.length()),
                                    m_ID.data()));
    }

    virtual inline void DrawUI() {
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
