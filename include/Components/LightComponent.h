#pragma once
#include <Core/Core.h>
#include <glm/glm.hpp>
#include <nlohmann/detail/macro_scope.hpp>

namespace Pyxis {

// The kinds of light the DeferredRenderer's lighting pass understands. The
// numeric values are sent to the lighting shader (DeferredLightFragment.hlsl)
// as the light's "type", so keep them in sync with it.
enum class LightType : int {
    // a circle of light around the entity, shadowed through the distance
    // field; optionally limited to a cone (spotlight)
    Point = 0,
    // flat, unshadowed light over the whole light quad (ambient)
    Diffuse = 1,
    // global / sun light: a direction (no position), shadowed by marching
    // toward the sun until open sky is found
    Directional = 2,
};

// A light in the world. Put it on an entity with a TransformComponent (the
// position of Point / Diffuse lights) and draw every one each frame with
// DeferredRenderer::DrawLight(light, position) before DrawLights.
struct LightComponent {
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(LightComponent, m_Enabled, m_Type, m_Color,
                                   m_Intensity, m_Radius, m_Falloff,
                                   m_SpotDirectionDegrees,
                                   m_SpotInnerAngleDegrees,
                                   m_SpotOuterAngleDegrees,
                                   m_SunDirectionDegrees);

    bool m_Enabled = true;
    LightType m_Type = LightType::Point;
    // RGBA, 0..1 (alpha unused by the lighting)
    glm::vec4 m_Color = {1, 1, 1, 1};
    float m_Intensity = 1.0f;

    // Point / Diffuse: radius of the light quad in world units (pixels).
    // Ignored by Directional lights (they cover the whole view).
    float m_Radius = 64.0f;
    // Point: attenuation curve, pow(1 - distance / radius, falloff)
    float m_Falloff = 1.0f;

    // Point (spotlight): the direction the cone points, in degrees
    // counter-clockwise from +x (world space; 90 = up, -90 = down)
    float m_SpotDirectionDegrees = -90.0f;
    // Point (spotlight): FULL cone angles in degrees. Inside the inner
    // angle the light is at full strength, it fades to 0 at the outer
    // angle. An outer angle of 360 (the default) is an ordinary,
    // unlimited point light.
    float m_SpotInnerAngleDegrees = 360.0f;
    float m_SpotOuterAngleDegrees = 360.0f;

    // Directional: the direction the sunlight TRAVELS, in degrees
    // counter-clockwise from +x (world space). -90 = straight down (sun
    // overhead), -45 = down and to the right (sun up-left). Rotate the sun
    // by changing this.
    float m_SunDirectionDegrees = -90.0f;
};

} // namespace Pyxis
