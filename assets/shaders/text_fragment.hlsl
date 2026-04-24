struct PSInput
{
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 main(PSInput input) : SV_Target0
{
    return Texture.Sample(Sampler, input.uv);
}
