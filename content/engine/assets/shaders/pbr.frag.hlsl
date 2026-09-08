Texture2D    base_color_tex         : register(t0, space2);
SamplerState base_color_samp        : register(s0, space2);
Texture2D    normal_tex             : register(t1, space2);
SamplerState normal_samp            : register(s1, space2);
Texture2D    metallic_roughness_tex : register(t2, space2);
SamplerState metallic_roughness_samp: register(s2, space2);
Texture2D    occlusion_tex          : register(t3, space2);
SamplerState occlusion_samp         : register(s3, space2);
Texture2D    emissive_tex           : register(t4, space2);
SamplerState emissive_samp          : register(s4, space2);
Texture2D              shadow_map   : register(t5, space2);
SamplerComparisonState shadow_samp  : register(s5, space2);
Texture2D    ssao_tex               : register(t6, space2);
SamplerState ssao_samp              : register(s6, space2);

cbuffer Material : register(b0, space3)
{
    float4 base_color;
    float3 emissive_color;
    float  emissive_strength;
    float2 uv_scale;
    float  metallic;
    float  roughness;
    float  ao_strength;
    float  normal_strength;
    float2 _pad;
};

cbuffer Engine : register(b1, space3)
{
    float3 light_direction;
    float3 light_color;
    float3 sky_color;
    float3 ground_color;
    float3 camera_position;
    float4x4 light_view_proj;
    float4 shadow_params;
    float4 screen_params;
};

struct PSInput
{
    float4 screen_pos    : SV_Position;
    float3 world_pos     : TEXCOORD0;
    float3 world_normal  : TEXCOORD1;
    float2 uv            : TEXCOORD2;
    float4 world_tangent : TEXCOORD3;
};

static const float PI = 3.14159265;

float3 srgb_to_linear(float3 c)
{
    return pow(c, 2.2);
}

float distribution_ggx(float ndoth, float alpha)
{
    float a2 = alpha * alpha;
    float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d + 1e-6);
}

float geometry_smith(float ndotv, float ndotl, float alpha)
{
    float k = (alpha + 1.0) * (alpha + 1.0) / 8.0;
    float gv = ndotv / (ndotv * (1.0 - k) + k);
    float gl = ndotl / (ndotl * (1.0 - k) + k);
    return gv * gl;
}

float3 fresnel_schlick(float vdoth, float3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - vdoth, 5.0);
}

float shadow_factor(float3 world_pos, float3 n, float ndotl)
{
    float texel_world = shadow_params.x;
    float bias = shadow_params.y;
    float strength = shadow_params.z;
    float texel_uv = shadow_params.w;

    float3 offset_pos = world_pos + n * texel_world * (1.0 + 2.0 * (1.0 - ndotl));
    float4 light_clip = mul(light_view_proj, float4(offset_pos, 1.0));
    float3 ndc = light_clip.xyz / light_clip.w;
    float2 uv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || ndc.z > 1.0)
    {
        return 1.0;
    }

    float depth = ndc.z - bias;
    float lit = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            lit += shadow_map.SampleCmpLevelZero(shadow_samp, uv + float2(x, y) * texel_uv, depth);
        }
    }

    return lerp(1.0, lit / 9.0, strength);
}

float3 shade_normal(PSInput input, float2 uv)
{
    float3 n = normalize(input.world_normal);
    float3 t = input.world_tangent.xyz;
    t = normalize(t - n * dot(n, t));
    float3 b = cross(n, t) * input.world_tangent.w;

    float3 n_ts = normal_tex.Sample(normal_samp, uv).xyz * 2.0 - 1.0;
    n_ts.xy *= normal_strength;
    return normalize(t * n_ts.x + b * n_ts.y + n * n_ts.z);
}

float4 main(PSInput input) : SV_Target0
{
    float2 uv = input.uv * uv_scale;

    float4 base_sample = base_color_tex.Sample(base_color_samp, uv);
    float3 albedo = srgb_to_linear(base_color.rgb * base_sample.rgb);

    float2 mr = metallic_roughness_tex.Sample(metallic_roughness_samp, uv).bg;
    float metal = saturate(metallic * mr.x);
    float rough = clamp(roughness * mr.y, 0.04, 1.0);
    float alpha = rough * rough;

    float occlusion = lerp(1.0, occlusion_tex.Sample(occlusion_samp, uv).r, ao_strength);
    occlusion *= ssao_tex.Sample(ssao_samp, input.screen_pos.xy * screen_params.xy).r;

    float3 n = shade_normal(input, uv);
    float3 v = normalize(camera_position - input.world_pos);
    float3 l = -normalize(light_direction);
    float3 h = normalize(l + v);

    float ndotl = saturate(dot(n, l));
    float ndotv = max(dot(n, v), 1e-4);
    float ndoth = saturate(dot(n, h));
    float vdoth = saturate(dot(v, h));

    float3 f0 = lerp(0.04, albedo, metal);
    float3 f = fresnel_schlick(vdoth, f0);
    float d = distribution_ggx(ndoth, alpha);
    float g = geometry_smith(ndotv, ndotl, alpha);
    float3 specular = (d * g * f) / (4.0 * ndotv * ndotl + 1e-4);
    float3 kd = (1.0 - f) * (1.0 - metal);
    float3 direct = (kd * albedo / PI + specular) * light_color * ndotl;
    direct *= shadow_factor(input.world_pos, n, ndotl);

    float3 hemisphere = lerp(ground_color, sky_color, n.y * 0.5 + 0.5);
    float3 ambient = hemisphere * (albedo * (1.0 - metal) + f0 * (1.0 - rough) * 0.5) * occlusion;

    float3 emissive = srgb_to_linear(emissive_tex.Sample(emissive_samp, uv).rgb) * emissive_color * emissive_strength;

    return float4(direct + ambient + emissive, base_color.a * base_sample.a);
}
