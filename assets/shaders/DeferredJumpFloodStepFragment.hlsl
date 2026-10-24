// Jump flood FLOOD pass (full-screen, padded size), run log2(size) times
// with the step size halving down to 1 (e.g. 512, 256, ..., 2, 1).
// Reads the previous result (RGBA16F: rg = nearest foreground occluder
// seed, ba = nearest open sky seed, (-1, -1) = none) and, for each of the
// two fields independently, looks at the 3x3 texels `stepSize` apart
// (itself included) and keeps the seed nearest to THIS texel.
// After the last pass every texel holds (approximately; JFA can rarely
// pick a slightly farther seed) its nearest seed of each kind.
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      JumpFloodSampler : register(s0, space2);
Texture2D<float4> JumpFloodInput   : register(t0, space2);

cbuffer JumpFloodStep : register(b0, space3)
{
    int stepSize; // texels between the sampled neighbours
    int3 padding;
};

float4 main(PSInput input) : SV_Target0
{
    int2 texel = int2(input.position.xy);
    float2 self = float2(texel);
    uint width, height;
    JumpFloodInput.GetDimensions(width, height);
    int2 maxTexel = int2(width, height) - 1;

    float2 bestForeground = float2(-1, -1);
    float bestForegroundDistanceSquared = 1e30;
    float2 bestSky = float2(-1, -1);
    float bestSkyDistanceSquared = 1e30;

    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            int2 neighbour = texel + int2(x, y) * stepSize;
            // outside the target: nothing to read
            if (any(neighbour < 0) || any(neighbour > maxTexel))
                continue;
            float4 seeds = JumpFloodInput.Load(int3(neighbour, 0));

            // foreground occluder seed (rg)
            if (seeds.x >= 0)
            {
                float2 offset = seeds.xy - self;
                float distanceSquared = dot(offset, offset);
                if (distanceSquared < bestForegroundDistanceSquared)
                {
                    bestForegroundDistanceSquared = distanceSquared;
                    bestForeground = seeds.xy;
                }
            }
            // open sky seed (ba)
            if (seeds.z >= 0)
            {
                float2 offset = seeds.zw - self;
                float distanceSquared = dot(offset, offset);
                if (distanceSquared < bestSkyDistanceSquared)
                {
                    bestSkyDistanceSquared = distanceSquared;
                    bestSky = seeds.zw;
                }
            }
        }
    }
    return float4(bestForeground, bestSky);
}
