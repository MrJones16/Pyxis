// Debug view of the distance field (DeferredRenderer::
// SetDebugShowDistanceField): replaces the lit image.
//   red   = distance to the nearest foreground occluder
//   green = distance to the nearest open sky
// both fade from bright (near) to dark over 64 texels, with a darker band
// every 16 texels so the field's shape is visible. Occluders / sky (distance
// 0) are black in their channel; blue marks texels with no seed at all.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      DistanceSampler : register(s0, space2);
Texture2D<float2> DistanceField   : register(t0, space2);

float Visualize(float distance)
{
    if (distance <= 0.0)
        return 0.0;
    float fade = 1.0 - saturate(distance / 64.0) * 0.8;
    float band = frac(distance / 16.0) < 0.1 ? 0.5 : 1.0;
    return fade * band;
}

float4 main(PSInput input) : SV_Target0
{
    float2 distances = DistanceField.Load(int3(input.position.xy, 0));
    float none = (distances.x > 9000.0 || distances.y > 9000.0) ? 0.5 : 0.0;
    return float4(Visualize(distances.x), Visualize(distances.y), none, 1.0);
}
