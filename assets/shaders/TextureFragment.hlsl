struct PSInput
{
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float4 overlay  : COLOR1;   // rgb mixed over the result by a (0 = none)
};

Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

float4 main(PSInput input) : SV_Target0
{
    float4 c = Texture.Sample(Sampler, input.uv) * input.color;
    if (c.a == 0) discard;
    c.rgb = lerp(c.rgb, input.overlay.rgb, input.overlay.a);
    return c;
}
