cbuffer MeshVS : register(b0, space1)
{
    float4x4 view_proj;
    float4x4 model;
    float4x4 normal_matrix;
};

struct VSInput
{
    float3 pos : TEXCOORD0;
};

float4 main(VSInput input) : SV_Position
{
    return mul(view_proj, mul(model, float4(input.pos, 1.0)));
}
