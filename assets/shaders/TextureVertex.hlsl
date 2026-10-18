struct VSInput
{
    float3 position : POSITION0;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float4 overlay  : COLOR1;   // UI color overlay, rgb mixed in by a
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
    float4 overlay  : COLOR1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    output.color    = input.color;
    output.overlay  = input.overlay;
    return output;
}

