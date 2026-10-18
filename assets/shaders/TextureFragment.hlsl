struct PSInput
{
    float2 uv           : TEXCOORD0;
    float4 color        : COLOR0;
    float4 overlay      : COLOR1;    // rgb mixed over the result by a (0 = none)
    float4 shapeRect    : TEXCOORD1; // UI shape: position in rect xy, size zw
    float4 cornerRadii  : TEXCOORD2; // tl, tr, br, bl
    float4 borderWidths : TEXCOORD3; // l, r, t, b (0 = filled)
    float4 shapeParams  : TEXCOORD4; // x = corner pixel size, y = shape on
};

Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

// signed distance from p to a rounded rect at the origin with the given
// size (y down) and per-corner radii (tl, tr, br, bl); <= 0 is inside
float RoundedRectDistance(float2 p, float2 size, float4 radii)
{
    float2 halfSize = size * 0.5;
    float2 q = p - halfSize;
    float radius = q.x < 0 ? (q.y < 0 ? radii.x : radii.w)
                           : (q.y < 0 ? radii.y : radii.z);
    float2 d = abs(q) - halfSize + radius;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radius;
}

float4 main(PSInput input) : SV_Target0
{
    if (input.shapeParams.y > 0.5)
    {
        // hard-edged (pixel art) mask, evaluated at the center of the
        // corner pixel grid cell this pixel is in
        float2 p = input.shapeRect.xy;
        float cell = max(input.shapeParams.x, 1.0);
        p = (floor(p / cell) + 0.5) * cell;
        float2 size = input.shapeRect.zw;
        if (RoundedRectDistance(p, size, input.cornerRadii) > 0)
            discard;
        float4 b = input.borderWidths; // l, r, t, b
        if (any(b > 0))
        {
            // border ring: drop the inner rounded rect
            float2 innerSize = size - float2(b.x + b.y, b.z + b.w);
            float4 innerRadii = max(input.cornerRadii -
                                        float4(max(b.x, b.z), max(b.y, b.z),
                                               max(b.y, b.w), max(b.x, b.w)),
                                    0.0);
            if (innerSize.x > 0 && innerSize.y > 0 &&
                RoundedRectDistance(p - float2(b.x, b.z), innerSize,
                                    innerRadii) <= 0)
                discard;
        }
    }
    float4 c = Texture.Sample(Sampler, input.uv) * input.color;
    if (c.a == 0) discard;
    c.rgb = lerp(c.rgb, input.overlay.rgb, input.overlay.a);
    return c;
}
