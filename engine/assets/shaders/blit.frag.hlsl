Texture2D    src_tex  : register(t0, space2);
SamplerState src_samp : register(s0, space2);

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    return src_tex.Sample(src_samp, uv);
}
