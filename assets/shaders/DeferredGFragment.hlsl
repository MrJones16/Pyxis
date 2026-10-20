// Default G-pass fragment shader.
// Material slots (space2): t0 albedo (RGBA), t1 normal map (RGBA8 tangent
// space: rgb = normal * 0.5 + 0.5, a = normal strength), t2 emission map
// (R16 float, multiplier on albedo). Missing maps are bound to 1x1 defaults
// by the DeferredRenderer (flat normal, emission 0).
struct PSInput
{
    float4 color             : COLOR0;
    float2 uv                : TEXCOORD0;
    float4 tangent_bitangent : TEXCOORD1;  // world tangent.xy, bitangent.xy
    float3 world_position    : TEXCOORD2;
};

Texture2D<float4> AlbedoTexture   : register(t0, space2);
Texture2D<float4> NormalTexture   : register(t1, space2);
Texture2D<float4> EmissionTexture : register(t2, space2);
SamplerState AlbedoSampler        : register(s0, space2);
SamplerState NormalSampler        : register(s1, space2);
SamplerState EmissionSampler      : register(s2, space2);

struct PSOutput
{
    float4 color       : SV_Target0;  // albedo (RGBA8)
    float4 normal_uv   : SV_Target1;  // world normal.xy (z is rebuilt), uv.zw
    float4 position_ns : SV_Target2;  // world position xyz, w unused
    float  emission    : SV_Target3;  // emission multiplier on albedo (R16F)
};

PSOutput main(PSInput input)
{
    PSOutput output;

    float4 albedo = AlbedoTexture.Sample(AlbedoSampler, input.uv) * input.color;
    // fully transparent pixels write nothing (no depth, normal or position),
    // so they can't hide what is drawn behind them later
    if (albedo.a < 0.001)
        discard;

    // tangent space normal, blended from flat by the strength in alpha
    float4 normalSample = NormalTexture.Sample(NormalSampler, input.uv);
    float3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    tangentNormal = lerp(float3(0, 0, 1), tangentNormal, normalSample.a);
    tangentNormal = normalize(tangentNormal);
    // into world space: tangent / bitangent carry the quad's rotation / flip
    float2 worldNormalXY = input.tangent_bitangent.xy * tangentNormal.x +
                           input.tangent_bitangent.zw * tangentNormal.y;

    output.color       = albedo;
    output.normal_uv   = float4(worldNormalXY, input.uv);
    output.position_ns = float4(input.world_position, 1.0);
    output.emission    = EmissionTexture.Sample(EmissionSampler, input.uv).r;

    return output;
}
