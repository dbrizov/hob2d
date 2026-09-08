Texture2D    src_tex  : register(t0, space2);
SamplerState src_samp : register(s0, space2);

float3 downsample_13_tap(Texture2D tex, SamplerState samp, float2 uv, float2 texel_size)
{
    float3 a = tex.Sample(samp, uv + texel_size * float2(-2.0, -2.0)).rgb;
    float3 b = tex.Sample(samp, uv + texel_size * float2( 0.0, -2.0)).rgb;
    float3 c = tex.Sample(samp, uv + texel_size * float2( 2.0, -2.0)).rgb;
    float3 d = tex.Sample(samp, uv + texel_size * float2(-2.0,  0.0)).rgb;
    float3 e = tex.Sample(samp, uv).rgb;
    float3 f = tex.Sample(samp, uv + texel_size * float2( 2.0,  0.0)).rgb;
    float3 g = tex.Sample(samp, uv + texel_size * float2(-2.0,  2.0)).rgb;
    float3 h = tex.Sample(samp, uv + texel_size * float2( 0.0,  2.0)).rgb;
    float3 i = tex.Sample(samp, uv + texel_size * float2( 2.0,  2.0)).rgb;
    float3 j = tex.Sample(samp, uv + texel_size * float2(-1.0, -1.0)).rgb;
    float3 k = tex.Sample(samp, uv + texel_size * float2( 1.0, -1.0)).rgb;
    float3 l = tex.Sample(samp, uv + texel_size * float2(-1.0,  1.0)).rgb;
    float3 m = tex.Sample(samp, uv + texel_size * float2( 1.0,  1.0)).rgb;

    return e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
}

cbuffer Bloom : register(b0, space3)
{
    float2 texel_size;
    float  threshold;
    float  knee;
};

float3 soft_threshold(float3 color)
{
    float brightness = max(color.r, max(color.g, color.b));
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4);
    return color * contribution;
}

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float3 color = downsample_13_tap(src_tex, src_samp, uv, texel_size);
    return float4(soft_threshold(color), 1.0);
}
