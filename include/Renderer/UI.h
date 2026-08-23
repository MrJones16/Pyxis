#pragma once
#include <Core/Timestep.h>
#include <Renderer/Text.h>
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

    UIModule(const std::string &id, Clay_ElementDeclaration config)
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

    virtual void
    DrawUI() const = 0; // abstract, must be implemented in children
};

// Basic module inherited from UIModule, good starting place
struct UIContainerModule : UIModule {
    UIContainerModule(const std::string &id, Clay_ElementDeclaration config)
        : UIModule(id, config) {};

    virtual inline void DrawUI() const override {
        if (!m_Enabled)
            return;

        CLAY(GetClayID(), m_Config) {
            for (auto &module : m_Children) {
                module->DrawUI();
            }
        }
    }
};

struct UIButtonModule : UIModule {
    UIButtonModule(const std::string &id, Clay_ElementDeclaration config,
                   std::function<void()> onClickFunction)
        : UIModule(id, config), m_OnClickFunction(onClickFunction) {};

    std::function<void()> m_OnClickFunction = nullptr;

    static void HandleButtonInteraction(Clay_ElementId elementId,
                                        Clay_PointerData pointerInfo,
                                        void *userData) {
        //  Pointer state allows you to detect mouse down / hold / release
        if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
            if (userData != nullptr) {
                UIButtonModule *buttonModule = (UIButtonModule *)userData;
                buttonModule->m_OnClickFunction();
            }
        }
    }

    virtual inline void DrawUI() const override {
        if (!m_Enabled)
            return;
        CLAY(GetClayID(), m_Config) {
            Clay_OnHover(UIButtonModule::HandleButtonInteraction, (void *)this);
            for (auto &module : m_Children) {
                module->DrawUI();
            }
        }
    }
};

struct UITextModule : UIModule {
    std::string m_Text;
    Clay_TextElementConfig m_TextConfig;
    // will set the config's userdata to be the fontID for you.
    UITextModule(const std::string &id, Clay_ElementDeclaration config,
                 const std::string &text, Clay_TextElementConfig textConfig)
        : UIModule(id, config), m_Text(text), m_TextConfig(textConfig) {};

    virtual inline void DrawUI() const override {
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

class UI {
  private:
  public:
    static void Init();
    static void OnWindowResize(const glm::vec2 &resolution);
    static void OnMouseWheelEvent(const glm::vec2 &mouseWheel);
    static void BeginUILayout();
    static Clay_RenderCommandArray EndUILayout(const Timestep &ts);
};
} // namespace Pyxis
