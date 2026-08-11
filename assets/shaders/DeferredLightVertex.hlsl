struct VSInput
{
    float4 color                      : COLOR0;     // loc 0
    float4 position                   : POSITION0;  // loc 1  (quad vertex)
    float4 position_center            : TEXCOORD0;  // loc 2
    float4 rad_intensity_falloff_type : TEXCOORD1;  // loc 3
};

struct VSOutput
{
    float4 position                   : SV_POSITION;
    float4 color                      : COLOR0;
    float4 position_center            : TEXCOORD0;
    float4 rad_intensity_falloff_type : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.position                   = input.position;          // already in clip space
    o.color                      = input.color;
    o.position_center            = input.position_center;
    o.rad_intensity_falloff_type = input.rad_intensity_falloff_type;
    return o;
}
