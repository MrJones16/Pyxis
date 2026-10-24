// Jump flood SEED pass (full-screen, padded size, first pass after the
// G-pass). Turns the occlusion texture into the starting point of two jump
// floods at once, written to an RGBA16F ping-pong texture:
//   rg = this texel's own coordinate if it is a FOREGROUND occluder
//        (occlusion.r > 0: anything that blocks some light), else (-1, -1)
//   ba = this texel's own coordinate if it is OPEN SKY (no foreground and
//        no wall: occlusion.r == 0 and occlusion.g == 0), else (-1, -1)
// (-1, -1) is the "no seed known yet" sentinel.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      OcclusionSampler : register(s0, space2);
// RG8: r = foreground opacity (0..1), g = wall / background opacity (0..1)
Texture2D<float4> GBufferOcclusion : register(t0, space2);

// below half an 8-bit step counts as zero
static const float OpacityEpsilon = 0.5 / 255.0;

float4 main(PSInput input) : SV_Target0
{
    int2 texel = int2(input.position.xy);
    float2 occlusion = GBufferOcclusion.Load(int3(texel, 0)).rg;

    float2 ownCoordinate = float2(texel);
    float2 foregroundSeed = occlusion.r > OpacityEpsilon ? ownCoordinate
                                                         : float2(-1, -1);
    bool openSky = occlusion.r <= OpacityEpsilon && occlusion.g <= OpacityEpsilon;
    float2 skySeed = openSky ? ownCoordinate : float2(-1, -1);
    return float4(foregroundSeed, skySeed);
}
