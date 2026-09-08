Texture2D    hdr_tex    : register(t0, space2);
SamplerState hdr_samp   : register(s0, space2);
Texture2D    bloom_tex  : register(t1, space2);
SamplerState bloom_samp : register(s1, space2);

cbuffer Tonemap : register(b0, space3)
{
    float exposure;
    float bloom_intensity;
    float2 _pad;
};

float3 aces_fitted(float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float3 hdr = hdr_tex.Sample(hdr_samp, uv).rgb;
    hdr += bloom_tex.Sample(bloom_samp, uv).rgb * bloom_intensity;
    hdr *= exposure;
    float3 ldr = aces_fitted(hdr);
    return float4(pow(ldr, 1.0 / 2.2), 1.0);
}
