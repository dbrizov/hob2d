Texture2D    albedo_tex  : register(t0, space2);
SamplerState albedo_samp : register(s0, space2);

cbuffer Material : register(b0, space3)
{
    float4 tint;
    float  use_albedo_texture;
};

cbuffer Engine : register(b1, space3)
{
    float3 light_direction;
    float3 light_color;
    float3 ambient_color;
};

struct PSInput
{
    float3 world_normal : TEXCOORD0;
    float2 uv           : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    float3 n = normalize(input.world_normal);
    float ndotl = max(dot(n, -normalize(light_direction)), 0.0);

    float4 albedo = tint;
    if (use_albedo_texture > 0.5)
    {
        albedo *= albedo_tex.Sample(albedo_samp, input.uv);
    }
    albedo.rgb = pow(albedo.rgb, 2.2);

    float3 lit = albedo.rgb * (ambient_color + light_color * ndotl);
    return float4(lit, albedo.a);
}
