struct VSInput
{
    float3 position     : POSITION0;
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    float4 overlay      : COLOR1;    // UI color overlay, rgb mixed in by a
    float4 shapeRect    : TEXCOORD1; // UI shape: position in rect xy, size zw
    float4 cornerRadii  : TEXCOORD2; // tl, tr, br, bl
    float4 borderWidths : TEXCOORD3; // l, r, t, b (0 = filled)
    float4 shapeParams  : TEXCOORD4; // x = corner pixel size, y = shape on
};

struct VSOutput
{
    float4 position     : SV_POSITION;
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    float4 overlay      : COLOR1;
    float4 shapeRect    : TEXCOORD1;
    float4 cornerRadii  : TEXCOORD2;
    float4 borderWidths : TEXCOORD3;
    float4 shapeParams  : TEXCOORD4;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.uv = input.uv;
    output.color    = input.color;
    output.overlay  = input.overlay;
    output.shapeRect    = input.shapeRect;
    output.cornerRadii  = input.cornerRadii;
    output.borderWidths = input.borderWidths;
    output.shapeParams  = input.shapeParams;
    return output;
}
