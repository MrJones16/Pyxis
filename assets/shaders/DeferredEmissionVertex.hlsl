// Emission composite vertex shader: a full-screen quad already in clip space.
struct VSInput
{
    float4 position : POSITION0;  // location 0, clip space
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = input.position;
    return output;
}
