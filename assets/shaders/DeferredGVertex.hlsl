struct VSInput
{
    float4 color             : COLOR0;     // location 0, tint
    float2 uv                : TEXCOORD0;  // location 1
    float4 tangent_bitangent : TEXCOORD1;  // location 2, world tangent.xy, bitangent.xy
    float3 position          : POSITION0;  // location 3, world position
};

struct VSOutput
{
    float4 position          : SV_POSITION;
    float4 color             : COLOR0;
    float2 uv                : TEXCOORD0;
    float4 tangent_bitangent : TEXCOORD1;
    float3 world_position    : TEXCOORD2;  // written to the G-buffer
};

cbuffer CameraConstants : register(b0, space1)  // slot 0
{
    float4x4 viewProj;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position          = mul(viewProj, float4(input.position, 1.0f));
    output.color             = input.color;
    output.uv                = input.uv;
    output.tangent_bitangent = input.tangent_bitangent;
    output.world_position    = input.position;
    return output;
}
