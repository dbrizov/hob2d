Texture2D    hdr_tex    : register(t0, space2);
SamplerState hdr_samp   : register(s0, space2);
Texture2D    fog_tex    : register(t1, space2);
SamplerState fog_samp   : register(s1, space2);
Texture2D    depth_tex  : register(t2, space2);
SamplerState depth_samp : register(s2, space2);

cbuffer FogUpsample : register(b0, space3)
{
    float4x4 inverse_view_proj;
    float3   camera_position;
    float    _pad0;
    float2   fog_texel_size;
    float2   _pad1;
};

float view_distance(float2 uv)
{
    float depth = depth_tex.Sample(depth_samp, uv).r;
    float4 ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 world = mul(inverse_view_proj, ndc);
    return distance(world.xyz / world.w, camera_position);
}

float4 main(float2 uv : TEXCOORD0) : SV_Target0
{
    float center_distance = view_distance(uv);
    float2 base = (floor(uv / fog_texel_size - 0.5) + 0.5) * fog_texel_size;

    float4 fog = 0.0;
    float weight_sum = 0.0;
    [unroll]
    for (int y = 0; y < 2; ++y)
    {
        [unroll]
        for (int x = 0; x < 2; ++x)
        {
            float2 tap = base + float2(x, y) * fog_texel_size;
            float distance_delta = abs(view_distance(tap) - center_distance);
            float weight = 1.0 / (1.0 + distance_delta * 4.0);
            fog += fog_tex.Sample(fog_samp, tap) * weight;
            weight_sum += weight;
        }
    }
    fog /= max(weight_sum, 1e-4);

    float3 color = hdr_tex.Sample(hdr_samp, uv).rgb;
    return float4(color * fog.a + fog.rgb, 1.0);
}
