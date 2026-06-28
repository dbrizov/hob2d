cbuffer UiVS : register(b0, space1)
{
    float4x4 proj;
    float2 translation;
};

struct VSInput
{
    float2 pos   : TEXCOORD0;
    float4 color : TEXCOORD1;
    float2 uv    : TEXCOORD2;
};

struct VSOutput
{
    float4 pos   : SV_Position;
    float4 color : TEXCOORD0;
    float2 uv    : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(proj, float4(input.pos + translation, 0.0, 1.0));
    o.color = input.color;
    o.uv = input.uv;
    return o;
}
