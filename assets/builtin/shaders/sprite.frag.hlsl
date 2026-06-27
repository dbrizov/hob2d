// Sprite fragment shader.
//
// Samples the bound texture and multiplies by the per-draw tint. If outline_width > 0,
// transparent texels (a <= alpha_threshold) that have any opaque neighbor are painted
// with outline_color - produces a silhouette outline. outline_width is measured in
// source texels; the engine pushes texel_size = 1 / texture_size so thickness stays
// consistent across textures of different resolutions. With outline_width == 0 this
// degenerates to a plain textured-quad shader.

Texture2D    sprite_tex  : register(t0, space2);
SamplerState sprite_samp : register(s0, space2);

cbuffer Engine : register(b0, space3)
{
    float2 texel_size;
    float  time;
};

cbuffer Material : register(b1, space3)
{
    float4 tint;
    float4 outline_color;
    float  outline_width;
    float  alpha_threshold;
};

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float4 center = sprite_tex.Sample(sprite_samp, uv);
    if (center.a > alpha_threshold)
    {
        return center * tint;
    }

    if (outline_width <= 0.0)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float ox = outline_width * texel_size.x;
    float oy = outline_width * texel_size.y;

    float2 offs[8] = {
        float2( ox, 0.0),
        float2(-ox, 0.0),
        float2(0.0,  oy),
        float2(0.0, -oy),
        float2( ox,  oy),
        float2( ox, -oy),
        float2(-ox,  oy),
        float2(-ox, -oy),
    };

    float max_neighbor_a = 0.0;
    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        max_neighbor_a = max(max_neighbor_a, sprite_tex.Sample(sprite_samp, uv + offs[i]).a);
    }

    if (max_neighbor_a > alpha_threshold)
    {
        return outline_color;
    }

    return float4(0.0, 0.0, 0.0, 0.0);
}
