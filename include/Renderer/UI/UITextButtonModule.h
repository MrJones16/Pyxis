#pragma once

#include <Renderer/Material.h>
#include <Renderer/UI/UIModule.h>

namespace Pyxis {
// note: Use std::bind to set the function pointer with preset arguments, and
// for member functions.
struct UITextButtonModule : UIModule {
    UITextButtonModule(const std::string &id, Clay_ElementDeclaration config,
                       Clay_TextElementConfig textConfig,
                       std::function<void()> onClickFunction = nullptr,
                       const std::string &text = "Button")
        : UIModule(id, config), m_OnClickFunction(onClickFunction),
          m_Text(text), m_TextConfig(textConfig) {};

  private:
    bool m_PressedState = false;

  public:
    std::string m_Text = "Button";
    Clay_TextElementConfig m_TextConfig = {};
    glm::vec2 m_PressedTextOffset = {0, 5};
    Ref<Material> m_Material = nullptr, m_MaterialPressed = nullptr;
    std::function<void()> m_OnClickFunction = nullptr;

    static void HandleButtonInteraction(Clay_ElementId elementId,
                                        Clay_PointerData pointerInfo,
                                        void *userData) {
        UITextButtonModule *buttonModule = (UITextButtonModule *)userData;
        if (userData == nullptr)
            return;

        if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME ||
            pointerInfo.state == CLAY_POINTER_DATA_PRESSED) {
            buttonModule->m_PressedState = true;
        } else if (pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
            if (buttonModule->m_OnClickFunction != nullptr)
                buttonModule->m_OnClickFunction();
        }
    }

    virtual inline void DrawUI() override {
        if (!m_Enabled)
            return;

        if (m_PressedState) {
            if (m_MaterialPressed != nullptr) {
                m_Config.image.imageData = m_MaterialPressed.get();
                m_Config.layout.padding.top = 2 * m_PressedTextOffset.y;
                m_Config.layout.padding.left = 2 * m_PressedTextOffset.x;
            }
        } else {
            if (m_Material != nullptr) {
                m_Config.image.imageData = m_Material.get();
                m_Config.layout.padding.left = 0;
                m_Config.layout.padding.top = 0;
            }
        }

        CLAY(GetClayID(), m_Config) {
            m_PressedState = false;
            Clay_OnHover(UITextButtonModule::HandleButtonInteraction,
                         (void *)this);
            Clay_String s{false, static_cast<int32_t>(m_Text.length()),
                          m_Text.data()};
            CLAY_TEXT(s, m_TextConfig);
            for (auto &module : m_Children) {
                module->DrawUI();
            }
        }
    }
};
} // namespace Pyxis
