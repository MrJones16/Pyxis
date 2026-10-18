struct VSInput
{
    float4 color     : COLOR0;     // location 0
    float4 normal_uv : TEXCOORD0;  // location 1  (normal.xy, uv.wz)
    float4 position_ns  : POSITION0;  // location 2
};

struct VSOutput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float4 normal_uv : TEXCOORD0;
    float4 pos_ns    : TEXCOORD1;   // interpolated position for the G-buffer + normal strength
};

cbuffer CameraConstants : register(b0, space1)  // slot 0
{
    float4x4 viewProj;
    // ...
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProj, float4(input.position_ns.xyz, 1.0f));
    output.color     = input.color;
    output.normal_uv = input.normal_uv;
    output.pos_ns       = input.position_ns;   // store the same value in the G-buffer
    return output;
}
