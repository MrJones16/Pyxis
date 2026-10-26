// G-buffer post-process: edge normals. Runs after the G-pass (before the
// distance field and lighting) and writes a new NormalUV target.
// For foreground pixels (occlusion.r) near open space it finds the outward
// direction (distance weighted gradient of the opacity over a
// (2 * width + 1)^2 window; at width 1 this is a 3x3 Sobel-like kernel)
// and the distance to the nearest open texel, then bends the stored normal
// toward that direction, fading out over `edgeWidthTexels`.
// Optionally the same for wall pixels (occlusion.g with no foreground).
// UV channels (zw) are kept; empty / interior pixels are copied unchanged.
//
// G-buffer stage bindings: t0 albedo, t1 normal/uv, t2 position,
// t3 emission, t4 occlusion; settings in b0 (space3).
struct PSInput
{
    float4 position : SV_POSITION;
};

SamplerState      AlbedoSampler     : register(s0, space2);
Texture2D<float4> GBufferAlbedo     : register(t0, space2);
SamplerState      NormalUVSampler   : register(s1, space2);
Texture2D<float4> GBufferNormalUV   : register(t1, space2);
SamplerState      PositionSampler   : register(s2, space2);
Texture2D<float4> GBufferPosition   : register(t2, space2);
SamplerState      EmissionSampler   : register(s3, space2);
Texture2D<float4> GBufferEmission   : register(t3, space2);
SamplerState      OcclusionSampler  : register(s4, space2);
Texture2D<float4> GBufferOcclusion  : register(t4, space2);

cbuffer EdgeNormalSettings : register(b0, space3)
{
    float strength;        // 0..1
    float edgeWidthTexels; // 1..8
    float applyToWalls;    // 1 = also walls
    float padding;
};

float4 main(PSInput input) : SV_Target0
{
    int2 texel = int2(input.position.xy);
    float4 normalUV = GBufferNormalUV.Load(int3(texel, 0));
    float2 occlusion = GBufferOcclusion.Load(int3(texel, 0)).rg;

    // never true: keeps every G-buffer input bound (resource reflection
    // counts must match the bound slots t0..t4)
    if (padding > 1e30)
        return GBufferAlbedo.Load(int3(texel, 0)) +
               GBufferPosition.Load(int3(texel, 0)) +
               GBufferEmission.Load(int3(texel, 0));

    bool foreground = occlusion.r > 0.5;
    bool wall = !foreground && occlusion.g > 0.5 && applyToWalls > 0.5;
    if (!foreground && !wall)
        return normalUV;

    uint width, height;
    GBufferOcclusion.GetDimensions(width, height);
    int2 maxTexel = int2(width, height) - 1;
    int radius = (int)ceil(clamp(edgeWidthTexels, 1.0, 8.0));

    float2 outward = float2(0, 0); // texel space (+y down)
    float nearestOpen = 1e9;
    for (int y = -radius; y <= radius; y++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            if (x == 0 && y == 0)
                continue;
            int2 neighbour = texel + int2(x, y);
            if (any(neighbour < 0) || any(neighbour > maxTexel))
                continue;
            float2 neighbourOcclusion =
                GBufferOcclusion.Load(int3(neighbour, 0)).rg;
            // foreground: open = no foreground. walls: open = neither
            float solid = foreground
                ? neighbourOcclusion.r
                : max(neighbourOcclusion.r, neighbourOcclusion.g);
            float open = 1.0 - solid;
            if (open <= 0.5)
                continue;
            float2 offset = float2(x, y);
            float distanceSquared = dot(offset, offset);
            nearestOpen = min(nearestOpen, sqrt(distanceSquared));
            outward += offset * (open / (distanceSquared * sqrt(distanceSquared)));
        }
    }
    if (nearestOpen > edgeWidthTexels + 0.5 || dot(outward, outward) <= 1e-8)
        return normalUV;

    float weight = strength * saturate(1.0 - (nearestOpen - 1.0) / edgeWidthTexels);
    // texel space -> world space (+y up)
    float2 outwardWorld = normalize(outward * float2(1, -1));
    float3 normal = float3(normalUV.xy,
                           sqrt(saturate(1.0 - dot(normalUV.xy, normalUV.xy))));
    float3 bent = lerp(normal, float3(outwardWorld, 0), weight);
    float bentLength = length(bent);
    if (bentLength <= 1e-5)
        return normalUV;
    bent /= bentLength;
    return float4(bent.xy, normalUV.zw);
}
