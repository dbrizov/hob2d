cbuffer MeshVS : register(b0, space1)
{
    float4x4 view_proj;
    float4x4 model;
    float4x4 normal_matrix;
};

struct VSInput
{
    float3 pos     : TEXCOORD0;
    float3 normal  : TEXCOORD1;
    float2 uv      : TEXCOORD2;
    float4 tangent : TEXCOORD3;
};

struct VSOutput
{
    float4 pos           : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float4 world_tangent : TEXCOORD3;
};

VSOutput main(VSInput input)
{
    float4 world = mul(model, float4(input.pos, 1.0));

    VSOutput o;
    o.pos = mul(view_proj, world);
    o.world_pos = world.xyz;
    o.world_normal = normalize(mul((float3x3)normal_matrix, input.normal));
    o.uv = input.uv;
    o.world_tangent = float4(normalize(mul((float3x3)model, input.tangent.xyz)), input.tangent.w);
    return o;
}
