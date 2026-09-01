#pragma once

#include <Renderer/Material.h>
#include <UI/UIModule.h>

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
    glm::vec2 m_TextOffset{0, 5};
    glm::vec2 m_PressedTextOffset = {0, 10};
    Ref<Bindable> m_Texture = nullptr, m_TexturePressed = nullptr;
    std::function<void()> m_OnClickFunction = nullptr;

    virtual inline void DrawUI() override {
        if (!m_Enabled)
            return;

        m_Config.layout.padding.top = 0;
        m_Config.layout.padding.left = 0;
        if (m_PressedState) {
            if (m_TexturePressed != nullptr) {
                m_Config.image.imageData = m_TexturePressed.get();
                m_Config.layout.padding.left = m_PressedTextOffset.x;
                m_Config.layout.padding.top = m_PressedTextOffset.y;
            }
        } else {
            if (m_Texture != nullptr) {
                m_Config.image.imageData = m_Texture.get();
                m_Config.layout.padding.left = m_TextOffset.x;
                m_Config.layout.padding.top = m_TextOffset.y;
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

  private:
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
};
} // namespace Pyxis
