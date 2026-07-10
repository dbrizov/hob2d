cbuffer LineVS : register(b0, space1)
{
    float4x4 proj;
};

struct VSInput
{
    float2 pos   : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct VSOutput
{
    float4 pos   : SV_Position;
    float4 color : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(proj, float4(input.pos, 0.0, 1.0));
    o.color = input.color;
    return o;
}
