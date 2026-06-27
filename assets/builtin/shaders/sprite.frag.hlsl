// Sprite fragment shader.
//
// Samples the bound texture and multiplies by the per-draw tint. Texels with
// alpha <= alpha_threshold are discarded so the silhouette stays crisp.

Texture2D    sprite_tex  : register(t0, space2);
SamplerState sprite_samp : register(s0, space2);

cbuffer Material : register(b0, space3)
{
    float4 tint;
    float  alpha_threshold;
};

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float4 center = sprite_tex.Sample(sprite_samp, uv);
    if (center.a <= alpha_threshold)
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    return center * tint;
}
