// Light quad vertex shader: every light is a quad in world space (a square
// of radius * 2 around Point / Diffuse lights, a huge view-centered quad
// for Directional lights), drawn additively into the lighting texture.
struct VSInput
{
    float4 color                      : COLOR0;     // loc 0
    float4 position                   : POSITION0;  // loc 1  (quad vertex) // world space
    float4 position_center            : TEXCOORD0;  // loc 2 // world space
    float4 rad_intensity_falloff_type : TEXCOORD1;  // loc 3
    // loc 4: Point: spotlight direction xy (world), cos(inner half angle),
    // cos(outer half angle). Directional: direction the light travels xy.
    float4 direction_cone             : TEXCOORD2;
};

struct VSOutput
{
    float4 position                   : SV_POSITION;// will be in clip space
    float4 color                      : COLOR0;
    float4 position_center            : TEXCOORD0;//in world space
    float4 rad_intensity_falloff_type : TEXCOORD1;
    float4 clip_position              : TEXCOORD2; //in clip space
    float4 direction_cone             : TEXCOORD3; // as given (world space)
    // xy: the light center in clip space (NDC, w = 1 for the orthographic
    // camera); zw: the light direction (direction_cone.xy) in clip space
    // (w = 0 vector). The fragment shader turns both into render texels to
    // march the shadow rays through the distance field.
    float4 center_direction_clip      : TEXCOORD4;
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
    o.direction_cone             = input.direction_cone;

    float4 centerClip    = mul(viewProj, float4(input.position_center.xyz, 1.0f));
    float4 directionClip = mul(viewProj, float4(input.direction_cone.xy, 0.0f, 0.0f));
    o.center_direction_clip = float4(centerClip.xy / centerClip.w, directionClip.xy);
    return o;
}
