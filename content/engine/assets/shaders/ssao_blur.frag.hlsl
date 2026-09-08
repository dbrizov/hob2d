Texture2D    ao_tex     : register(t0, space2);
SamplerState ao_samp    : register(s0, space2);
Texture2D    depth_tex  : register(t1, space2);
SamplerState depth_samp : register(s1, space2);

cbuffer Blur : register(b0, space3)
{
    float4x4 inverse_view_proj;
    float3   camera_position;
    float    _pad0;
    float2   texel_size;
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
    float threshold = max(0.25, center_distance * 0.05);

    float sum = 0.0;
    float weight = 0.0;
    [unroll]
    for (int y = -2; y < 2; ++y)
    {
        [unroll]
        for (int x = -2; x < 2; ++x)
        {
            float2 offset = float2(x + 0.5, y + 0.5) * texel_size;
            float2 sample_uv = uv + offset;
            float w = abs(view_distance(sample_uv) - center_distance) < threshold ? 1.0 : 0.0;
            sum += ao_tex.Sample(ao_samp, sample_uv).r * w;
            weight += w;
        }
    }

    float ao = weight > 0.0 ? sum / weight : ao_tex.Sample(ao_samp, uv).r;
    return float4(ao, ao, ao, 1.0);
}
