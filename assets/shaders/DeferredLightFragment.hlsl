struct PSInput
{
    float4 position                   : SV_POSITION; // got converted to pixel space, resolution dependent
    float4 color                      : COLOR0;
    float4 position_center            : TEXCOORD0;//world space
    float4 rad_intensity_falloff_type : TEXCOORD1;
    float4 clip_position              : TEXCOORD2; // clip position
};

// G-buffer
SamplerState      Sampler0        : register(s0, space2);
SamplerState      Sampler1        : register(s1, space2);
SamplerState      Sampler2        : register(s2, space2);
Texture2D<float4> GBufferColor    : register(t0, space2);
Texture2D<float4> GBufferNormalUV : register(t1, space2);
Texture2D<float4> GBufferPositionNS : register(t2, space2);

float4 main(PSInput input) : SV_Target0
{
    // SV_POSITION is this pixel's coordinate in the render target (origin
    // top left, like the G-buffer textures): read the G-buffer texel directly
    int3 texel = int3(input.position.xy, 0);

    float4 albedo   = GBufferColor.Load(texel);
    float4 normalUV = GBufferNormalUV.Load(texel);
    float4 pos      = GBufferPositionNS.Load(texel);

    float3 normal = normalize(float3(normalUV.xy, 0)); // reconstruct if you stored only xy
    // if you stored a full normal you would do normal = normalize(normalUV.xyz);

    float  radius    = input.rad_intensity_falloff_type.x;
    float  intensity = input.rad_intensity_falloff_type.y;
    float  falloff   = input.rad_intensity_falloff_type.z;
    float  type      = input.rad_intensity_falloff_type.w;

    float3 lightColor = input.color.rgb * intensity;

    // ---------- Point light (type ≈ 0) ----------
    if (type < 0.25)
    {

        float3 lightPos   = float3(input.position_center.xy, 0);
        float3 surfacePos = float3(pos.xy, 0);

        float3  L         = lightPos - surfacePos;
        float   dist      = length(L);
        float3  lightDir  = L / max(dist, 0.001);


        // simple smooth attenuation
        float atten = saturate(1.0 - dist / radius);
        atten = pow(atten, falloff);           // falloff controls the curve

        float NdotL = saturate(dot(normal, lightDir));
        float normalStrength = pos.w;
        NdotL = (NdotL * normalStrength) + (1 - normalStrength); 
        return float4(albedo.rgb * lightColor * NdotL * atten, 1.0);
    }

    // ---------- Diffuse / area / full-rect light (type ≥ 0.5) ----------
    else
    {
        // just lights the whole quad with the given color
        // (no distance attenuation, no normal influence)
        return float4(albedo.rgb * lightColor, 1.0);
    }
}
