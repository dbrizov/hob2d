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
};

struct VSOutput
{
    float4 pos          : SV_Position;
    float3 world_normal : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(view_proj, mul(model, float4(input.pos, 1.0)));
    o.world_normal = normalize(mul((float3x3)normal_matrix, input.normal));
    return o;
}
