// Emission composite: drawn once per frame, additively into the lighting
// texture (before the lights): adds the G-buffer emission rgb (emitted
// light, independent of the albedo) once, regardless of how many lights
// there are. It also writes the lit image's coverage alpha: 1 where the
// G-pass drew something (PositionWS.w > 0), 0 where it drew nothing, so the
// lit image is transparent over empty pixels when DrawToScreen alpha blends
// it over the background (the lighting texture is cleared to alpha 0).
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      Sampler0        : register(s0, space2);
Texture2D<float4> GBufferEmission : register(t0, space2);
SamplerState      Sampler1        : register(s1, space2);
Texture2D<float4> GBufferPosition : register(t1, space2);

float4 main(PSInput input) : SV_Target0
{
    int3 texel = int3(input.position.xy, 0);
    float3 emission = GBufferEmission.Load(texel).rgb;
    float coverage = GBufferPosition.Load(texel).w > 0 ? 1.0 : 0.0;
    return float4(emission, coverage);
}
