struct PSInput
{
    float4 color     : COLOR0;
    float4 normal_uv : TEXCOORD0;  // normal.xy, uv.wz
    float4 pos_ns    : TEXCOORD1;
};

Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler      : register(s0, space2);

struct PSOutput
{
    float4 color       : SV_Target0;  // Color (vec4)
    float4 normal_uv   : SV_Target1;  // NormalUV (vec4)
    float4 position_ns : SV_Target2;  // Position_NS (vec4)
};

PSOutput main(PSInput input)
{
    PSOutput output;

    // Sample using the packed UV (.wz as you specified)
    float4 albedo = Texture.Sample(Sampler, input.normal_uv.zw) * input.color;

    output.color     = albedo;
    output.normal_uv = input.normal_uv;   // pass through (or normalize .xy if desired)
    output.position_ns  = input.pos_ns;

    return output;
}
