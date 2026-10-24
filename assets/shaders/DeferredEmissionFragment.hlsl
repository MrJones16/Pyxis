// Emission composite: drawn once per frame, additively into the lighting
// texture (before the lights): adds the G-buffer emission rgb (emitted
// light, independent of the albedo) once, regardless of how many lights
// there are.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      Sampler0        : register(s0, space2);
Texture2D<float4> GBufferEmission : register(t0, space2);

float4 main(PSInput input) : SV_Target0
{
    int3 texel = int3(input.position.xy, 0);
    float3 emission = GBufferEmission.Load(texel).rgb;
    // alpha 0: the lit image's alpha is left to the lights
    return float4(emission, 0.0);
}
