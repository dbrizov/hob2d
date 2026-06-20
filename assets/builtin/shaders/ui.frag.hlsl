Texture2D    tex  : register(t0, space2);
SamplerState samp : register(s0, space2);

struct FSInput
{
    float4 color : TEXCOORD0;
    float2 uv    : TEXCOORD1;
};

float4 main(FSInput input) : SV_Target0
{
    return input.color * tex.Sample(samp, input.uv);
}
