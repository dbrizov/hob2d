Texture2D    src_tex  : register(t0, space2);
SamplerState src_samp : register(s0, space2);

cbuffer Bloom : register(b0, space3)
{
    float2 texel_size;
    float  radius;
    float  _pad;
};

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float2 o = texel_size * radius;
    float3 sum = src_tex.Sample(src_samp, uv + float2(-o.x, -o.y)).rgb;
    sum += src_tex.Sample(src_samp, uv + float2(0.0, -o.y)).rgb * 2.0;
    sum += src_tex.Sample(src_samp, uv + float2(o.x, -o.y)).rgb;
    sum += src_tex.Sample(src_samp, uv + float2(-o.x, 0.0)).rgb * 2.0;
    sum += src_tex.Sample(src_samp, uv).rgb * 4.0;
    sum += src_tex.Sample(src_samp, uv + float2(o.x, 0.0)).rgb * 2.0;
    sum += src_tex.Sample(src_samp, uv + float2(-o.x, o.y)).rgb;
    sum += src_tex.Sample(src_samp, uv + float2(0.0, o.y)).rgb * 2.0;
    sum += src_tex.Sample(src_samp, uv + float2(o.x, o.y)).rgb;
    return float4(sum / 16.0, 1.0);
}
