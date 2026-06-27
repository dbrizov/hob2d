Texture2D    sprite_tex   : register(t0, space2);
SamplerState sprite_samp  : register(s0, space2);
Texture2D    distort_tex  : register(t1, space2);
SamplerState distort_samp : register(s1, space2);

cbuffer Engine : register(b0, space3)
{
    float2 texel_size;
    float  time;
};

cbuffer Material : register(b1, space3)
{
    float4 tint;
    float  alpha_threshold;
    float  distort_strength; // max UV offset, in UV units
    float  distort_speed;    // how fast the noise map scrolls
};

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    // Scroll the noise map over time and remap RG from [0,1] to [-1,1]. frac() wraps the
    // lookup so the (tileable) noise map repeats — the sprite sampler is clamp-to-edge.
    float2 scroll = float2(time * distort_speed * 0.05, time * distort_speed * 0.03);
    float2 flow = distort_tex.Sample(distort_samp, frac(uv + scroll)).rg * 2.0 - 1.0;

    float2 duv = uv + flow * distort_strength;

    float4 base = sprite_tex.Sample(sprite_samp, duv);
    if (base.a <= alpha_threshold)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    return base * tint;
}
