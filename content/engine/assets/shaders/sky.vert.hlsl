cbuffer SkyVS : register(b0, space1)
{
    float4x4 inverse_view_proj;
    float3   camera_position;
    float    _pad;
};

struct VSOutput
{
    float4 pos       : SV_Position;
    float3 world_dir : TEXCOORD0;
};

VSOutput main(uint vid : SV_VertexID)
{
    float2 uv = float2((vid << 1) & 2, vid & 2);
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float4 world = mul(inverse_view_proj, float4(ndc, 1.0, 1.0));

    VSOutput o;
    o.pos = float4(ndc, 1.0, 1.0);
    o.world_dir = world.xyz / world.w - camera_position;
    return o;
}
