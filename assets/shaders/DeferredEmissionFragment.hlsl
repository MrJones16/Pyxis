// Emission composite: drawn once per frame, additively into the lighting
// texture (before the lights): adds albedo.rgb * emission, independent of
// how many lights there are.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState     Sampler0       : register(s0, space2);
SamplerState     Sampler1       : register(s1, space2);
Texture2D<float4> GBufferColor    : register(t0, space2);
Texture2D<float4> GBufferEmission : register(t1, space2);

float4 main(PSInput input) : SV_Target0
{
    int3 texel = int3(input.position.xy, 0);
    float3 albedo   = GBufferColor.Load(texel).rgb;
    float  emission = GBufferEmission.Load(texel).r;
    // alpha 0: the lit image's alpha is left to the lights
    return float4(albedo * emission, 0.0);
}
