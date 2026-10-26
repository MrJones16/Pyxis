// Parallax background layer (full-screen quad over the padded lighting
// target, alpha blended). Drawn by DeferredRenderer::DrawLights after the
// lights, only on pixels the G-pass left empty (PositionWS.w == 0), one
// draw per layer, back to front.
// t0 = layer texture, t1 = G-buffer PositionWS.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      LayerSampler    : register(s0, space2);
Texture2D<float4> LayerTexture    : register(t0, space2);
SamplerState      PositionSampler : register(s1, space2);
Texture2D<float4> GBufferPosition : register(t1, space2);

cbuffer ParallaxLayer : register(b0, space3)
{
    // RGBA multiplier (tint, already times the sun colour)
    float4 color;
    // layer space position (world units, +y up) of the top-left corner of
    // padded texel (0, 0), already snapped to whole render texels
    float2 layerOrigin;
    // world units per render texel
    float2 worldPerTexel;
    // world size of one repetition of the texture
    float2 tileWorldSize;
    // 1 = repeat along that axis, 0 = clamp to the edge texels
    float2 repeatAxes;
};

float4 main(PSInput input) : SV_Target0
{
    int2 texel = int2(input.position.xy);
    // something was drawn here in the G-pass: the world covers the layer
    if (GBufferPosition.Load(int3(texel, 0)).w > 0)
        discard;

    // texel center in layer space (+y up), then texture uv (+v down)
    float2 layerPosition = layerOrigin +
        (float2(texel) + 0.5) * worldPerTexel * float2(1, -1);
    float2 uv = layerPosition / tileWorldSize * float2(1, -1);
    uv.x = repeatAxes.x > 0.5 ? frac(uv.x) : saturate(uv.x);
    uv.y = repeatAxes.y > 0.5 ? frac(uv.y) : saturate(uv.y);

    // point lookup (crisp texels, no filtering across the wrap seam)
    uint width, height;
    LayerTexture.GetDimensions(width, height);
    int2 layerTexel = min(int2(uv * float2(width, height)),
                          int2(width, height) - 1);
    float4 sampleColor = LayerTexture.Load(int3(layerTexel, 0)) * color;
    if (sampleColor.a <= 0)
        discard;
    return sampleColor;
}
