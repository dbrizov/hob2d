Texture2D    depth_tex   : register(t0, space2);
SamplerState depth_samp  : register(s0, space2);
Texture2D    normal_tex  : register(t1, space2);
SamplerState normal_samp : register(s1, space2);

cbuffer SSAO : register(b0, space3)
{
    float4x4 view_proj;
    float4x4 inverse_view_proj;
    float3   camera_position;
    float    radius;
    float2   texel_size;
    float    bias;
    float    intensity;
};

static const int SAMPLE_COUNT = 16;
static const float3 KERNEL[SAMPLE_COUNT] =
{
    float3( 0.0537,  0.0117,  0.0325), float3(-0.0473, -0.0703,  0.0555), float3( 0.1125, -0.0602,  0.0813),
    float3(-0.1372,  0.0966,  0.0648), float3( 0.0405,  0.1799,  0.1102), float3( 0.2013, -0.1524,  0.0561),
    float3(-0.2489, -0.1053,  0.1614), float3( 0.0778,  0.2837,  0.1997), float3( 0.3163,  0.1466,  0.1381),
    float3(-0.3474,  0.2201,  0.1105), float3(-0.0845, -0.4149,  0.2107), float3( 0.4308, -0.2110,  0.2412),
    float3(-0.4911, -0.1735,  0.3102), float3( 0.2209,  0.5320,  0.2606), float3( 0.5876,  0.2418,  0.3305),
    float3(-0.3520,  0.6154,  0.4302),
};

float3 reconstruct_world(float2 uv, float depth)
{
    float4 ndc = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, depth, 1.0);
    float4 world = mul(inverse_view_proj, ndc);
    return world.xyz / world.w;
}

float interleaved_gradient_noise(float2 pixel)
{
    return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
}

float4 main(float4 screen_pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    float depth = depth_tex.Sample(depth_samp, uv).r;
    if (depth >= 1.0)
    {
        return float4(1.0, 1.0, 1.0, 1.0);
    }

    float3 origin = reconstruct_world(uv, depth);
    float3 n = normalize(normal_tex.Sample(normal_samp, uv).xyz);
    float origin_distance = distance(origin, camera_position);

    float angle = interleaved_gradient_noise(screen_pos.xy) * 6.2831853;
    float3 helper = abs(n.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 t = normalize(cross(helper, n));
    float3 b = cross(n, t);
    float ca = cos(angle);
    float sa = sin(angle);
    float3 tangent = t * ca + b * sa;
    float3 bitangent = b * ca - t * sa;

    float occlusion = 0.0;
    [unroll]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float3 k = KERNEL[i];
        float3 sample_pos = origin + (tangent * k.x + bitangent * k.y + n * k.z) * radius;
        float4 clip = mul(view_proj, float4(sample_pos, 1.0));
        float3 sample_ndc = clip.xyz / clip.w;
        float2 sample_uv = float2(sample_ndc.x * 0.5 + 0.5, 0.5 - sample_ndc.y * 0.5);
        if (any(sample_uv < 0.0) || any(sample_uv > 1.0))
        {
            continue;
        }

        float scene_depth = depth_tex.Sample(depth_samp, sample_uv).r;
        float3 scene_pos = reconstruct_world(sample_uv, scene_depth);
        float sample_distance = distance(sample_pos, camera_position);
        float scene_distance = distance(scene_pos, camera_position);
        float range = smoothstep(0.0, 1.0, radius / max(abs(origin_distance - scene_distance), 1e-4));
        occlusion += (scene_distance < sample_distance - bias ? 1.0 : 0.0) * range;
    }

    float ao = saturate(1.0 - occlusion / SAMPLE_COUNT * intensity);
    return float4(ao, ao, ao, 1.0);
}
