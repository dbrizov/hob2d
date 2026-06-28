Texture2D    sprite_tex  : register(t0, space2);
SamplerState sprite_samp : register(s0, space2);

cbuffer Engine : register(b0, space3)
{
    float2 texel_size;
    float  game_time;
};

cbuffer Material : register(b1, space3)
{
    float4 tint;
    float  alpha_threshold;
};

// Rotate an RGB color around the (1,1,1)/sqrt(3) luminance axis by `angle` rad.
float3 hue_shift(float3 rgb, float angle)
{
    const float3 k = float3(0.57735, 0.57735, 0.57735); // normalize(1,1,1)
    float ca = cos(angle);
    float sa = sin(angle);
    return rgb * ca + cross(k, rgb) * sa + k * dot(k, rgb) * (1.0 - ca);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    // --- 1. Wavy UV warp -----------------------------------------------------
    // Amplitude is measured in source texels so the wobble looks the same on
    // small and large sprites. Sine phase advances with time so the warp ripples.
    float2 warp = float2(
        sin(uv.y * 25.0 + game_time * 4.0) * texel_size.x * 3.0,
        sin(uv.x * 20.0 + game_time * 3.0) * texel_size.y * 3.0
    );
    float2 wuv = uv + warp;

    // Silhouette gate — keep the original sprite shape, don't fringe into the
    // transparent border.
    float center_a = sprite_tex.Sample(sprite_samp, wuv).a;
    if (center_a <= alpha_threshold)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    // --- 2. Radial chromatic aberration --------------------------------------
    float2 from_center = wuv - 0.5;
    // Aberration grows with distance from sprite center and breathes with time.
    float ab_strength = 0.06 + 0.04 * sin(game_time * 2.0);
    float2 ab = from_center * length(from_center) * ab_strength;
    float r = sprite_tex.Sample(sprite_samp, wuv + ab).r;
    float g = sprite_tex.Sample(sprite_samp, wuv).g;
    float b = sprite_tex.Sample(sprite_samp, wuv - ab).b;
    float3 rgb = float3(r, g, b);

    // --- 3. Per-column hue rotation ------------------------------------------
    // Full 2*pi sweep across the sprite plus a time offset, so the rainbow
    // scrolls horizontally over the sprite.
    rgb = hue_shift(rgb, wuv.x * 6.28318 + game_time * 2.0);

    return float4(rgb, center_a) * tint;
}
