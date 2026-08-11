struct VSInput
{
    float4 color     : COLOR0;     // location 0
    float4 normal_uv : TEXCOORD0;  // location 1  (normal.xy, uv.wz)
    float4 position  : POSITION0;  // location 2
};

struct VSOutput
{
    float4 position  : SV_POSITION;
    float4 color     : COLOR0;
    float4 normal_uv : TEXCOORD0;
    float4 pos       : TEXCOORD1;   // interpolated position for the G-buffer
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position  = input.position;   // already float4
    output.color     = input.color;
    output.normal_uv = input.normal_uv;
    output.pos       = input.position;   // store the same value in the G-buffer
    return output;
}
