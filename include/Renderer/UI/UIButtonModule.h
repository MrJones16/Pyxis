#pragma once

#include <Renderer/Material.h>
#include <Renderer/UI/UIModule.h>

namespace Pyxis {
struct UIButtonModule : UIModule {
    UIButtonModule(const std::string &id, Clay_ElementDeclaration config,
                   std::function<void()> onClickFunction)
        : UIModule(id, config), m_OnClickFunction(onClickFunction) {};

  private:
    bool m_PressedState = false;

  public:
    Ref<Material> m_Material = nullptr, m_MaterialPressed = nullptr;
    std::function<void()> m_OnClickFunction = nullptr;

    static void HandleButtonInteraction(Clay_ElementId elementId,
                                        Clay_PointerData pointerInfo,
                                        void *userData) {
        //  Pointer state allows you to detect mouse down / hold / release

        UIButtonModule *buttonModule = (UIButtonModule *)userData;
        if (userData == nullptr)
            return;

        if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME ||
            pointerInfo.state == CLAY_POINTER_DATA_PRESSED) {
            buttonModule->m_PressedState = true;
        } else if (pointerInfo.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
            if (userData != nullptr) {
                buttonModule->m_OnClickFunction();
            }
        }
    }

    virtual inline void DrawUI() override {
        if (!m_Enabled)
            return;

        if (m_PressedState) {
            if (m_MaterialPressed != nullptr)
                m_Config.image.imageData = m_MaterialPressed.get();
        } else {
            if (m_Material != nullptr)
                m_Config.image.imageData = m_Material.get();
        }

        CLAY(GetClayID(), m_Config) {
            Clay_OnHover(UIButtonModule::HandleButtonInteraction, (void *)this);
            if (m_PressedState == true) {
            }
            for (auto &module : m_Children) {
                module->DrawUI();
            }
        }
        m_PressedState = false;
    }
};
} // namespace Pyxis
