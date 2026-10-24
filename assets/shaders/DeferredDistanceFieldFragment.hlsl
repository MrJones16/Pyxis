// Jump flood DISTANCE pass (full-screen, padded size, after the last flood
// pass). Turns the nearest seed coordinates into distances in texels,
// written to the RG16F distance field read by the lighting:
//   r = distance to the nearest FOREGROUND occluder (0 on an occluder)
//   g = distance to the nearest OPEN SKY texel (0 on open sky)
// DistanceFieldNone (10000) where no seed of that kind exists on screen.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      JumpFloodSampler : register(s0, space2);
Texture2D<float4> JumpFloodResult  : register(t0, space2);

static const float DistanceFieldNone = 10000.0;

float2 main(PSInput input) : SV_Target0
{
    int2 texel = int2(input.position.xy);
    float2 self = float2(texel);
    float4 seeds = JumpFloodResult.Load(int3(texel, 0));
    float foregroundDistance =
        seeds.x >= 0 ? length(seeds.xy - self) : DistanceFieldNone;
    float skyDistance =
        seeds.z >= 0 ? length(seeds.zw - self) : DistanceFieldNone;
    return float2(foregroundDistance, skyDistance);
}
