Texture2D              depth_tex   : register(t0, space2);
SamplerState           depth_samp  : register(s0, space2);
Texture2D              shadow_map  : register(t1, space2);
SamplerComparisonState shadow_samp : register(s1, space2);

cbuffer Fog : register(b0, space3)
{
    float4x4 inverse_view_proj;
    float4x4 light_view_proj;
    float3   camera_position;
    float    max_distance;
    float3   sun_direction;
    float    step_count;
    float3   sun_color;
    float    density;
    float3   fog_color;
    float    height;
    float    falloff;
    float    anisotropy;
    float    shadow_strength;
    float    shadow_bias;
};

static const float PI = 3.14159265;

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

float henyey_greenstein(float cos_theta, float g)
{
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(max(1.0 + g2 - 2.0 * g * cos_theta, 1e-4), 1.5));
}

float sun_visibility(float3 world_pos)
{
    if (shadow_strength <= 0.0)
    {
        return 1.0;
    }

    float4 light_clip = mul(light_view_proj, float4(world_pos, 1.0));
    float3 ndc = light_clip.xyz / light_clip.w;
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
    {
        return 1.0;
    }

    float lit = shadow_map.SampleCmpLevelZero(shadow_samp, uv, ndc.z - shadow_bias);
    return lerp(1.0, lit, shadow_strength);
}

float4 main(float4 screen_pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    float depth = depth_tex.Sample(depth_samp, uv).r;
    float3 view_dir = normalize(reconstruct_world(uv, 0.5) - camera_position);
    float3 end = depth < 1.0 ? reconstruct_world(uv, depth) : camera_position + view_dir * max_distance;
    float ray_length = min(distance(end, camera_position), max_distance);

    int steps = max((int)step_count, 4);
    float step_length = ray_length / steps;
    float jitter = interleaved_gradient_noise(screen_pos.xy);
    float3 to_sun = -normalize(sun_direction);
    float phase = henyey_greenstein(dot(view_dir, to_sun), anisotropy);

    float3 inscatter = 0.0;
    float transmittance = 1.0;
    for (int i = 0; i < steps; ++i)
    {
        float t = (i + jitter) * step_length;
        float3 p = camera_position + view_dir * t;
        float local_density = density * saturate(exp(-(p.y - height) / falloff));
        float sample_transmittance = exp(-local_density * step_length);
        float3 light = sun_color * phase * sun_visibility(p) + fog_color;
        inscatter += transmittance * (1.0 - sample_transmittance) * light;
        transmittance *= sample_transmittance;
    }

    return float4(inscatter, transmittance);
}
