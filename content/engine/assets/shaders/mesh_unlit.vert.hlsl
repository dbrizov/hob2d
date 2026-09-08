cbuffer MeshVS : register(b0, space1)
{
    float4x4 view_proj;
    float4x4 model;
    float4x4 normal_matrix;
};

struct VSInput
{
    float3 pos    : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv     : TEXCOORD2;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(view_proj, mul(model, float4(input.pos, 1.0)));
    o.uv = input.uv;
    return o;
}
