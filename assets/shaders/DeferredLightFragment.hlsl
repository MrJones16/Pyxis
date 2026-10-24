// Lighting: one invocation per pixel per light quad, added into the
// lighting texture (padded size). Reads the G-buffer and the distance field
// (built by the jump flood passes after the G-pass) and shadows Point and
// Directional lights by marching through the distance field.
//
// Light types (rad_intensity_falloff_type.w, see Pyxis::LightType):
//   0 Point       radius / falloff attenuation, optional spotlight cone,
//                 shadow ray toward the light
//   1 Diffuse     flat ambient color, no shadow
//   2 Directional sun: marches toward the sun until open sky is found
struct PSInput
{
    float4 position                   : SV_POSITION; // got converted to pixel space, resolution dependent
    float4 color                      : COLOR0;
    float4 position_center            : TEXCOORD0;//world space
    float4 rad_intensity_falloff_type : TEXCOORD1;
    float4 clip_position              : TEXCOORD2; // clip position
    float4 direction_cone             : TEXCOORD3; // world space direction xy, cos inner, cos outer
    float4 center_direction_clip      : TEXCOORD4; // light center (clip xy), direction (clip zw)
};

// G-buffer
SamplerState      Sampler0        : register(s0, space2);
SamplerState      Sampler1        : register(s1, space2);
SamplerState      Sampler2        : register(s2, space2);
SamplerState      Sampler3        : register(s3, space2);
SamplerState      Sampler4        : register(s4, space2);
Texture2D<float4> GBufferColor    : register(t0, space2);
Texture2D<float4> GBufferNormalUV : register(t1, space2);
Texture2D<float4> GBufferPositionWS : register(t2, space2);
// RG8: r = foreground opacity (0..1), g = wall / background opacity
Texture2D<float4> GBufferOcclusion : register(t3, space2);
// RG16F, in texels: r = distance to the nearest foreground occluder,
// g = distance to the nearest open sky (no foreground and no wall)
Texture2D<float2> DistanceField    : register(t4, space2);

// DeferredRenderer::LightingSettings (fragment uniform slot 0)
cbuffer LightingSettings : register(b0, space3)
{
    // occluding texels a ray crosses before they attenuate at full strength
    float bleedDistance;
    // max texels a Directional light marches looking for open sky
    float sunMaxDistance;
    // safety cap on march iterations
    float maxMarchSteps;
    float settingsPadding;
};

// below half an 8-bit step counts as zero
static const float OpacityEpsilon = 0.5 / 255.0;
// a ray carrying less light than this is fully shadowed
static const float TransmittanceCutoff = 0.01;
// Points on a shadow ray are anywhere inside a texel and the distance field
// is measured between texel centers, so the free space around a point is
// at least (distance - SafetyMargin): one texel diagonal (~1.41) rounded up.
static const float SafetyMargin = 1.5;

// clip space (NDC) -> render texel coordinates (origin top left, texel
// centers at .5, like SV_POSITION)
float2 ClipToTexel(float2 clip, float2 size)
{
    return float2((clip.x * 0.5 + 0.5) * size.x, (0.5 - clip.y * 0.5) * size.y);
}

// LIGHT BLEED: how strongly the n-th (0-based) occluding texel a ray
// crosses attenuates it. The first texels only partly count, ramping up
// to full strength after bleedDistance texels, so light soaks a few texels
// into a lit surface (the receiving texel itself is the first one crossed)
// and leaks a little through very thin solids. bleedDistance 0 = exact.
float BleedWeight(float solidTexelsCrossed)
{
    return saturate((solidTexelsCrossed + 1.0) / (bleedDistance + 1.0));
}

// One texel of a shadow ray: multiply the transmittance by
// (1 - opacity * bleed weight) if the texel occludes. Texels are only
// counted once (unit steps can land in the same texel twice).
void AttenuateTexel(int2 texel, inout int2 lastTexel, inout float transmittance,
                    inout float solidTexelsCrossed)
{
    if (all(texel == lastTexel))
        return;
    lastTexel = texel;
    float opacity = GBufferOcclusion.Load(int3(texel, 0)).r;
    if (opacity > OpacityEpsilon)
    {
        transmittance *= 1.0 - opacity * BleedWeight(solidTexelsCrossed);
        solidTexelsCrossed += 1.0;
    }
}

// Sphere traces from `start` to `target` (texels) and returns how much light
// gets through (1 = unoccluded, 0 = blocked).
// Far from occluders the distance field says how far the ray can jump
// without touching any: jump (distance - SafetyMargin). Close to one, step
// one texel at a time and attenuate by each texel's foreground opacity.
float TraceShadowRay(float2 start, float2 target, int2 size)
{
    float2 toTarget = target - start;
    float totalDistance = length(toTarget);
    if (totalDistance < 0.5)
        return 1.0;
    float2 direction = toTarget / totalDistance;

    float transmittance = 1.0;
    float solidTexelsCrossed = 0.0;
    int2 lastTexel = int2(-1, -1);
    float travelled = 0.0;
    int maxSteps = (int)maxMarchSteps;
    for (int step = 0; step < maxSteps; step++)
    {
        // reached the light's texel
        if (travelled >= totalDistance - 0.5)
            break;
        float2 position = start + direction * travelled;
        int2 texel = int2(floor(position));
        // left the padded targets: nothing known beyond, assume clear
        if (any(texel < 0) || any(texel >= size))
            break;
        float distanceToOccluder = DistanceField.Load(int3(texel, 0)).r;
        if (distanceToOccluder > SafetyMargin + 0.5)
        {
            travelled += distanceToOccluder - SafetyMargin;
            continue;
        }
        AttenuateTexel(texel, lastTexel, transmittance, solidTexelsCrossed);
        if (transmittance < TransmittanceCutoff)
            return 0.0;
        travelled += 1.0;
    }
    return transmittance;
}

// Marches from `start` toward the sun (`directionToSun`, texels, normalized)
// up to sunMaxDistance texels, and returns the light that reaches open sky
// (the transmittance through what was crossed), or 0 if no open sky
// (no foreground and no wall) is reached.
// Uses both distance fields: the foreground one (r) for jumping through air
// and the sky one (g) so jumps never skip over a patch of sky, and to stop
// early once no sky can be within the remaining distance.
float TraceSunRay(float2 start, float2 directionToSun, int2 size)
{
    float transmittance = 1.0;
    float solidTexelsCrossed = 0.0;
    int2 lastTexel = int2(-1, -1);
    float travelled = 0.0;
    int maxSteps = (int)maxMarchSteps;
    for (int step = 0; step < maxSteps; step++)
    {
        float2 position = start + directionToSun * travelled;
        int2 texel = int2(floor(position));
        // left the padded targets (only padding pixels can, while
        // sunMaxDistance <= RenderPadding): assume open sky beyond
        if (any(texel < 0) || any(texel >= size))
            return transmittance;
        float2 distances = DistanceField.Load(int3(texel, 0));
        float distanceToOccluder = distances.r;
        float distanceToSky = distances.g;
        // this texel is open sky: the sun reaches the pixel
        if (distanceToSky < 0.5)
            return transmittance;
        // the nearest sky (in ANY direction) is farther than the march
        // can still go: no sky will be found, shadowed
        if (travelled + distanceToSky - SafetyMargin > sunMaxDistance)
            return 0.0;
        // in air: jump, but not past the nearest occluder or sky
        float freeDistance = min(distanceToOccluder, distanceToSky);
        if (freeDistance > SafetyMargin + 0.5)
        {
            travelled += freeDistance - SafetyMargin;
            continue;
        }
        AttenuateTexel(texel, lastTexel, transmittance, solidTexelsCrossed);
        if (transmittance < TransmittanceCutoff)
            return 0.0;
        travelled += 1.0;
    }
    return 0.0;
}

float4 main(PSInput input) : SV_Target0
{
    // SV_POSITION is this pixel's coordinate in the render target (origin
    // top left, like the G-buffer textures): read the G-buffer texel directly
    int3 texel = int3(input.position.xy, 0);

    float4 albedo   = GBufferColor.Load(texel);
    float4 normalUV = GBufferNormalUV.Load(texel);
    float4 pos      = GBufferPositionWS.Load(texel);

    if (pos.w < 0.5) discard;

    uint width, height;
    DistanceField.GetDimensions(width, height);
    int2 size = int2(width, height);
    float2 sizeF = float2(size);

    // the G-buffer holds the world normal's xy; normals face the camera, so
    // z is rebuilt (flat surfaces are (0, 0, 1))
    float3 normal = float3(normalUV.xy,
                           sqrt(saturate(1.0 - dot(normalUV.xy, normalUV.xy))));

    float  radius    = input.rad_intensity_falloff_type.x;
    float  intensity = input.rad_intensity_falloff_type.y;
    float  falloff   = input.rad_intensity_falloff_type.z;
    float  type      = input.rad_intensity_falloff_type.w;

    float3 lightColor = input.color.rgb * intensity;

    // ---------- Point light / spotlight (type 0) ----------
    if (type < 0.5)
    {

        float3 lightPos   = float3(input.position_center.xy, 0);
        float3 surfacePos = float3(pos.xy, 0);

        float3  L         = lightPos - surfacePos;
        float   dist      = length(L);
        float3  lightDir  = L / max(dist, 0.001);


        // simple smooth attenuation
        float atten = saturate(1.0 - dist / radius);
        atten = pow(atten, falloff);           // falloff controls the curve
        if (atten <= 0.0) discard;

        // spotlight cone: full strength inside the inner half angle, fading
        // to 0 at the outer one (cos -1 = 180 degree half angle = unlimited)
        float cosInner = input.direction_cone.z;
        float cosOuter = input.direction_cone.w;
        float spot = 1.0;
        if (cosOuter > -0.9999)
        {
            float2 spotDirection = normalize(input.direction_cone.xy);
            // angle between the cone axis and light -> surface
            float cosAngle = dot(-lightDir.xy, spotDirection);
            spot = smoothstep(cosOuter, max(cosInner, cosOuter + 0.0001), cosAngle);
            if (spot <= 0.0) discard;
        }

        // the light sits in the plane: a flat normal (0, 0, 1) is fully
        // lit, a normal tilted away from it darkens
        float NdotL = saturate(normal.z + dot(normal.xy, lightDir.xy));

        // shadow: from this pixel to the light's texel
        float2 lightTexel = ClipToTexel(input.center_direction_clip.xy, sizeF);
        float transmittance = TraceShadowRay(input.position.xy, lightTexel, size);

        return float4(albedo.rgb * lightColor * NdotL * atten * spot * transmittance, 1.0);
    }

    // ---------- Diffuse / area / full-rect light (type 1) ----------
    else if (type < 1.5)
    {
        // just lights the whole quad with the given color
        // (no distance attenuation, no normal influence)
        return float4(albedo.rgb * lightColor, 1.0);
    }

    // ---------- Directional / sun light (type 2) ----------
    else
    {
        // direction_cone.xy is the direction the light travels; march the
        // other way, toward the sun. In texels: the clip space direction
        // scaled to the target, y flipped (texel y grows downward).
        float2 directionClip = input.center_direction_clip.zw;
        float2 directionTexel = float2(directionClip.x * sizeF.x, -directionClip.y * sizeF.y);
        if (dot(directionTexel, directionTexel) < 1e-8) discard;
        float2 directionToSunTexel = -normalize(directionTexel);

        float transmittance = TraceSunRay(input.position.xy, directionToSunTexel, size);
        if (transmittance <= 0.0) discard;

        float2 directionToSunWorld = -normalize(input.direction_cone.xy);
        float NdotL = saturate(normal.z + dot(normal.xy, directionToSunWorld));
        return float4(albedo.rgb * lightColor * NdotL * transmittance, 1.0);
    }
}
