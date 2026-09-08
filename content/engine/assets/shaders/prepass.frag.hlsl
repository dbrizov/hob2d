float4 main(float3 world_normal : TEXCOORD0) : SV_Target0
{
    return float4(normalize(world_normal), 1.0);
}
