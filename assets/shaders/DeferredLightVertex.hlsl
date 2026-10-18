struct VSInput
{
    float4 color                      : COLOR0;     // loc 0
    float4 position                   : POSITION0;  // loc 1  (quad vertex) // world space
    float4 position_center            : TEXCOORD0;  // loc 2 // world space
    float4 rad_intensity_falloff_type : TEXCOORD1;  // loc 3
};

struct VSOutput
{
    float4 position                   : SV_POSITION;// will be in clip space
    float4 color                      : COLOR0;
    float4 position_center            : TEXCOORD0;//in world space
    float4 rad_intensity_falloff_type : TEXCOORD1;
    float4 clip_position              : TEXCOORD2; //in clip space
};

cbuffer CameraConstants : register(b0, space1)  // slot 0
{
    float4x4 viewProj;
    // ...
};

VSOutput main(VSInput input)
{
    VSOutput o;

    //convert world space to clip space with camera matrix
    o.position = mul(viewProj, float4(input.position.xyz, 1.0f));

    o.color                      = input.color;
    o.position_center            = input.position_center;// in world space
    o.rad_intensity_falloff_type = input.rad_intensity_falloff_type;
    o.clip_position              = o.position;    //use clip space calculated before      
    return o;
}
