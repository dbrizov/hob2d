cbuffer DebugTextVS : register(b0, space1)
{
    float4x4 proj;
};

struct VSInput
{
    float2 pos   : TEXCOORD0;
    float2 uv    : TEXCOORD1;
    float4 color : TEXCOORD2;
};

struct VSOutput
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(proj, float4(input.pos, 0.0, 1.0));
    o.uv = input.uv;
    o.color = input.color;
    return o;
}
