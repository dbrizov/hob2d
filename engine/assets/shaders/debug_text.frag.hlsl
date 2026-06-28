Texture2D    atlas_tex  : register(t0, space2);
SamplerState atlas_samp : register(s0, space2);

struct FSInput
{
    float2 uv    : TEXCOORD0;
    float4 color : TEXCOORD1;
};

float4 main(FSInput input) : SV_Target0
{
    float4 sampled = atlas_tex.Sample(atlas_samp, input.uv);
    return float4(input.color.rgb, input.color.a * sampled.a);
}
