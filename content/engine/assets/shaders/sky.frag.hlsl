cbuffer Sky : register(b0, space3)
{
    float3 sky_color;
    float  _pad0;
    float3 ground_color;
    float  _pad1;
    float3 sun_direction;
    float  _pad2;
    float3 sun_color;
    float  _pad3;
};

float4 main(float3 world_dir : TEXCOORD0) : SV_Target0
{
    float3 d = normalize(world_dir);
    float3 horizon = lerp(ground_color, sky_color, 0.5) * 1.35;
    float3 color = d.y >= 0.0 ? lerp(horizon, sky_color, pow(d.y, 0.6)) : lerp(horizon, ground_color, pow(-d.y, 0.5));

    float3 l = -normalize(sun_direction);
    float cos_angle = dot(d, l);
    float disc = smoothstep(0.9995, 0.9999, cos_angle);
    float glow = pow(saturate(cos_angle), 96.0) * 0.12;
    float above_horizon = smoothstep(-0.08, 0.02, d.y);
    color += sun_color * (disc * 2.0 + glow) * above_horizon;

    return float4(color, 1.0);
}
