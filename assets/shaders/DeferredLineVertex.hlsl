// Debug line vertex shader: world position -> clip space with the camera.
struct VSInput
{
    float4 color    : COLOR0;     // location 0
    float4 position : POSITION0;  // location 1 (world xyz)
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
};

cbuffer CameraConstants : register(b0, space1)
{
    float4x4 viewProj;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProj, float4(input.position.xyz, 1.0f));
    output.color = input.color;
    return output;
}
