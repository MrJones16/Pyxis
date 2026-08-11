struct PSInput
{
    float4 position                   : SV_POSITION;
    float4 color                      : COLOR0;
    float4 position_center            : TEXCOORD0;
    float4 rad_intensity_falloff_type : TEXCOORD1;
};

// G-buffer
Texture2D<float4> GBufferColor    : register(t0, space2);
Texture2D<float4> GBufferNormalUV : register(t1, space2);
Texture2D<float4> GBufferPosition : register(t2, space2);
SamplerState      Sampler         : register(s0, space2);

float4 main(PSInput input) : SV_Target0
{
    // Screen-space UV from the current pixel
    float2 screenUV = input.position.xy;           // if you pass SV_Position, 
    // better: use SV_Position and divide by resolution, or pass it as a TEXCOORD

    // Recommended: add float4 position : SV_POSITION; and do:
    // float2 screenUV = input.position.xy / float2(screenWidth, screenHeight);
    // (or use a constant buffer for resolution)

    float4 albedo   = GBufferColor.Sample(Sampler, screenUV);
    float4 normalUV = GBufferNormalUV.Sample(Sampler, screenUV);
    float4 pos      = GBufferPosition.Sample(Sampler, screenUV);

    float3 normal = float3(normalUV.xy, 0.0); // reconstruct if you stored only xy
    // if you stored a full normal you would do normal = normalize(normalUV.xyz);

    float  radius    = input.rad_intensity_falloff_type.x;
    float  intensity = input.rad_intensity_falloff_type.y;
    float  falloff   = input.rad_intensity_falloff_type.z;
    float  type      = input.rad_intensity_falloff_type.w;

    float3 lightColor = input.color.rgb * intensity;

    // ---------- Point light (type ≈ 0) ----------
    if (type < 0.25)
    {
        float3 lightPos   = input.position_center.xyz;
        float3 surfacePos = pos.xyz;

        float3  L         = lightPos - surfacePos;
        float   dist      = length(L);
        float3  lightDir  = L / max(dist, 0.0001);

        // simple smooth attenuation
        float atten = saturate(1.0 - dist / radius);
        atten = pow(atten, falloff);           // falloff controls the curve

        float NdotL = saturate(dot(normal, lightDir));

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
