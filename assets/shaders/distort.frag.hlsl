Texture2D    sprite_tex   : register(t0, space2);
SamplerState sprite_samp  : register(s0, space2);
Texture2D    distort_tex  : register(t1, space2);
SamplerState distort_samp : register(s1, space2);

cbuffer Engine : register(b0, space3)
{
    float game_time;
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
    float2 scroll = float2(game_time * distort_speed * 0.05, game_time * distort_speed * 0.03);
    float2 flow = distort_tex.Sample(distort_samp, uv + scroll).rg * 2.0 - 1.0;

    float2 duv = uv + flow * distort_strength;

    float4 base = sprite_tex.Sample(sprite_samp, duv);
    if (base.a <= alpha_threshold)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    return base * tint;
}
