#pragma once
#include <UI/UIModule.h>

namespace Pyxis {
struct UITextModule : UIModule {
    std::string m_Text;
    Clay_TextElementConfig m_TextConfig;
    // will set the config's userdata to be the fontID for you.
    UITextModule(const std::string &id, Clay_ElementDeclaration config,
                 const std::string &text, Clay_TextElementConfig textConfig)
        : UIModule(id, config), m_Text(text), m_TextConfig(textConfig) {};

    virtual inline void DrawUI() override {
        if (!m_Enabled)
            return;
        Clay_String s{false, static_cast<int32_t>(m_Text.length()),
                      m_Text.data()};

        CLAY(GetClayID(), m_Config) {
            CLAY_TEXT(s, m_TextConfig);
            for (auto &module : m_Children) {
                module->DrawUI();
            }
        }
    }
};
} // namespace Pyxis
