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
    // 0 = linear bleed falloff, 1 = rescaled exponential (see BleedFalloff)
    float bleedCurve;
    // k of the exponential falloff exp(-k * x); higher = faster initial drop
    float bleedSharpness;
    float settingsPadding0;
    float settingsPadding1;
    float settingsPadding2;
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

// LIGHT BLEED: a shadow ray tracks two things over the texels it crosses
// (the receiving texel itself is NOT crossed, so a surface facing the light
// is fully lit):
//   beer  = product of (1 - opacity)        (Beer-Lambert-ish)
//   depth = sum of opacity                  (solid depth; translucent texels
//                                            count fractionally)
// and lets through
//   T = max(beer, BleedFalloff(saturate(depth / bleedDistance)))
// BleedFalloff(x) blends a linear and an exponential curve, both 1 at x = 0
// and exactly 0 at x = 1:
//   linear(x) = 1 - x
//   expo(x)   = (exp(-k x) - exp(-k)) / (1 - exp(-k))     k = bleedSharpness
//   falloff   = lerp(linear, expo, bleedCurve)
// expo drops fast near the surface then tails off softly: its slope at x = 1
// is only -k e^-k / (1 - e^-k) (~ -0.075 for k = 4) versus -1 for linear,
// so with bleedCurve = 1 there is no visible kink where the light reaches 0.
// Fully opaque material: beer = 0, so light falls to exactly 0 at
// bleedDistance texels deep. Translucent material (water, steam, smoke)
// always passes at least (1 - opacity) per texel. bleedDistance 0 = hard
// shadow: the linear term is 1 only while depth is 0, so any opaque texel
// blocks and translucent ones attenuate by plain Beer-Lambert.
float BleedFalloff(float x)
{
    float linearTerm = 1.0 - x;
    float k = bleedSharpness;
    if (k < 1e-3)
        return linearTerm; // exp curve degenerates to linear as k -> 0
    float tail = exp(-k);
    float expTerm = (exp(-k * x) - tail) / (1.0 - tail);
    return lerp(linearTerm, expTerm, saturate(bleedCurve));
}

float BleedTransmittance(float beer, float depth)
{
    float bleedTerm = bleedDistance > 0.0
                          ? BleedFalloff(saturate(depth / bleedDistance))
                          : (depth > 0.0 ? 0.0 : 1.0);
    return max(beer, bleedTerm);
}

// One occluding opacity sample of a shadow ray.
void AttenuateOpacity(float opacity, inout float beer, inout float depth)
{
    if (opacity > OpacityEpsilon)
    {
        beer *= 1.0 - saturate(opacity);
        depth += saturate(opacity);
    }
}

// One texel of a shadow ray. Texels are only counted once (re-entering the
// DDA after a distance check can start in the texel visited last).
void AttenuateTexel(int2 texel, inout int2 lastTexel, inout float beer,
                    inout float depth)
{
    if (all(texel == lastTexel))
        return;
    lastTexel = texel;
    AttenuateOpacity(GBufferOcclusion.Load(int3(texel, 0)).r, beer, depth);
}

// Near occluders the ray walks the texel grid with an Amanatides-Woo DDA,
// visiting every texel the ray passes through (unit steps along the ray
// skipped the corners of texels it clipped, so light leaked through
// diagonal gaps). After this many texels the distance field is checked
// again so the ray can go back to jumping once it is in open air.
static const int DdaCellsPerCheck = 4;
// tMaxX and tMaxY closer than this (in texels along the ray): the ray goes
// exactly through a texel corner
static const float CornerEpsilon = 1e-4;
// a ray (re)entering the grid starts this far past the jump / boundary
// point so floor() lands in the texel being entered
static const float EntryEpsilon = 1e-3;

struct DdaState
{
    int2   cell;    // texel the ray is in
    int2   stepDir; // +-1 per axis (0 if the ray is parallel to it)
    float2 tMax;    // ray distance (from start) to the next x / y texel boundary
    float2 tDelta;  // ray distance between two x / y boundaries
    float  t;       // ray distance at which the ray entered `cell`
};

// (Re)initializes the DDA at ray distance t (any float position).
DdaState InitDda(float2 start, float2 direction, float t)
{
    DdaState dda;
    float2 position = start + direction * t;
    dda.cell = int2(floor(position));
    dda.t = t;
    for (int axis = 0; axis < 2; axis++)
    {
        float d = direction[axis];
        if (abs(d) < 1e-6)
        {
            dda.stepDir[axis] = 0;
            dda.tDelta[axis] = 1e30;
            dda.tMax[axis] = 1e30;
        }
        else
        {
            dda.stepDir[axis] = d > 0.0 ? 1 : -1;
            dda.tDelta[axis] = abs(1.0 / d);
            float boundary = d > 0.0 ? floor(position[axis]) + 1.0 : floor(position[axis]);
            dda.tMax[axis] = t + (boundary - position[axis]) / d;
        }
    }
    return dda;
}

// Advances the DDA one texel. On an exact corner crossing (both boundaries
// at the same distance) the ray squeezes between the two side texels:
// attenuate by the stronger of them, so a diagonal pair of solid texels
// blocks the ray instead of letting it slip through the shared corner.
void StepDda(inout DdaState dda, int2 size, inout float beer,
             inout float depth)
{
    if (abs(dda.tMax.x - dda.tMax.y) < CornerEpsilon)
    {
        int2 sideX = dda.cell + int2(dda.stepDir.x, 0);
        int2 sideY = dda.cell + int2(0, dda.stepDir.y);
        float opacityX = (any(sideX < 0) || any(sideX >= size)) ? 0.0 : GBufferOcclusion.Load(int3(sideX, 0)).r;
        float opacityY = (any(sideY < 0) || any(sideY >= size)) ? 0.0 : GBufferOcclusion.Load(int3(sideY, 0)).r;
        AttenuateOpacity(max(opacityX, opacityY), beer, depth);
        dda.t = min(dda.tMax.x, dda.tMax.y);
        dda.cell += dda.stepDir;
        dda.tMax += dda.tDelta;
    }
    else if (dda.tMax.x < dda.tMax.y)
    {
        dda.t = dda.tMax.x;
        dda.cell.x += dda.stepDir.x;
        dda.tMax.x += dda.tDelta.x;
    }
    else
    {
        dda.t = dda.tMax.y;
        dda.cell.y += dda.stepDir.y;
        dda.tMax.y += dda.tDelta.y;
    }
}

// Traces from `start` to `target` (texels) and returns how much light
// gets through (1 = unoccluded, 0 = blocked).
// Hybrid: far from occluders the distance field says how far the ray can
// jump without touching any: jump (distance - SafetyMargin). Close to one,
// walk the texel grid with the DDA for DdaCellsPerCheck texels,
// attenuating by each texel's foreground opacity, then check the distance
// field again (re-initializing the DDA from the new float position).
float TraceShadowRay(float2 start, float2 target, int2 size)
{
    float2 toTarget = target - start;
    float totalDistance = length(toTarget);
    if (totalDistance < 0.5)
        return 1.0;
    float2 direction = toTarget / totalDistance;
    int2 targetTexel = int2(floor(target));

    float beer = 1.0;
    float depth = 0.0;
    // the receiving texel never attenuates its own light
    int2 lastTexel = int2(floor(start));
    float travelled = 0.0;
    bool inDda = false;
    int ddaCells = 0;
    DdaState dda = InitDda(start, direction, 0.0);
    int maxSteps = (int)maxMarchSteps;
    for (int step = 0; step < maxSteps; step++)
    {
        if (inDda)
            travelled = dda.t;
        // reached the light's texel
        if (travelled >= totalDistance - 0.5)
            break;
        int2 texel = inDda ? dda.cell : int2(floor(start + direction * travelled));
        if (all(texel == targetTexel))
            break;
        // left the padded targets: nothing known beyond, assume clear
        if (any(texel < 0) || any(texel >= size))
            break;
        if (!inDda)
        {
            float distanceToOccluder = DistanceField.Load(int3(texel, 0)).r;
            if (distanceToOccluder > SafetyMargin + 0.5)
            {
                travelled += distanceToOccluder - SafetyMargin;
                continue;
            }
            dda = InitDda(start, direction, travelled);
            inDda = true;
            ddaCells = 0;
        }
        AttenuateTexel(dda.cell, lastTexel, beer, depth);
        StepDda(dda, size, beer, depth);
        if (BleedTransmittance(beer, depth) < TransmittanceCutoff)
            return 0.0;
        if (++ddaCells >= DdaCellsPerCheck)
        {
            // recheck the distance field from inside the texel just entered
            inDda = false;
            travelled = dda.t + EntryEpsilon;
        }
    }
    return BleedTransmittance(beer, depth);
}

// Marches from `start` toward the sun (`directionToSun`, texels, normalized)
// up to sunMaxDistance texels, and returns the light that reaches open sky
// (the transmittance through what was crossed), or 0 if no open sky
// (no foreground and no wall) is reached.
// Uses both distance fields: the foreground one (r) for jumping through air
// and the sky one (g) so jumps never skip over a patch of sky, and to stop
// early once no sky can be within the remaining distance. Near occluders
// it walks the grid with the same hybrid DDA as TraceShadowRay, checking
// every texel visited for open sky.
float TraceSunRay(float2 start, float2 directionToSun, int2 size)
{
    float beer = 1.0;
    float depth = 0.0;
    // the receiving texel never attenuates its own light
    int2 lastTexel = int2(floor(start));
    float travelled = 0.0;
    bool inDda = false;
    int ddaCells = 0;
    DdaState dda = InitDda(start, directionToSun, 0.0);
    int maxSteps = (int)maxMarchSteps;
    for (int step = 0; step < maxSteps; step++)
    {
        if (inDda)
            travelled = dda.t;
        int2 texel = inDda ? dda.cell : int2(floor(start + directionToSun * travelled));
        // left the padded targets (only padding pixels can, while
        // sunMaxDistance <= RenderPadding): assume open sky beyond
        if (any(texel < 0) || any(texel >= size))
            return BleedTransmittance(beer, depth);
        float2 distances = DistanceField.Load(int3(texel, 0));
        float distanceToOccluder = distances.r;
        float distanceToSky = distances.g;
        // this texel is open sky: the sun reaches the pixel
        if (distanceToSky < 0.5)
            return BleedTransmittance(beer, depth);
        // the nearest sky (in ANY direction) is farther than the march
        // can still go: no sky will be found, shadowed
        if (travelled + distanceToSky - SafetyMargin > sunMaxDistance)
            return 0.0;
        if (!inDda)
        {
            // in air: jump, but not past the nearest occluder or sky
            float freeDistance = min(distanceToOccluder, distanceToSky);
            if (freeDistance > SafetyMargin + 0.5)
            {
                travelled += freeDistance - SafetyMargin;
                continue;
            }
            dda = InitDda(start, directionToSun, travelled);
            inDda = true;
            ddaCells = 0;
        }
        AttenuateTexel(dda.cell, lastTexel, beer, depth);
        StepDda(dda, size, beer, depth);
        if (BleedTransmittance(beer, depth) < TransmittanceCutoff)
            return 0.0;
        if (++ddaCells >= DdaCellsPerCheck)
        {
            inDda = false;
            travelled = dda.t + EntryEpsilon;
        }
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
