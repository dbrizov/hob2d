Texture2D    albedo_tex  : register(t0, space2);
SamplerState albedo_samp : register(s0, space2);

cbuffer Material : register(b0, space3)
{
    float4 tint;
    float  use_albedo_texture;
};

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float4 color = tint;
    if (use_albedo_texture > 0.5)
    {
        color *= albedo_tex.Sample(albedo_samp, uv);
    }
    color.rgb = pow(color.rgb, 2.2);
    return color;
}
