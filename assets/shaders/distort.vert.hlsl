cbuffer SpriteVS : register(b0, space1)
{
    float4x4 view_proj;
    float2 world_pos;
    float2 size;
    float2 pivot;
    float rotation;
    float _pad;
};

struct VSInput
{
    float2 pos : TEXCOORD0;
    float2 uv  : TEXCOORD1;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    float2 q = input.pos - pivot;
    float2 local = float2(size.x * q.x, -size.y * q.y);

    float c = cos(rotation);
    float s = sin(rotation);
    float2 r = float2(c * local.x - s * local.y, s * local.x + c * local.y);

    float2 world = world_pos + r;

    VSOutput o;
    o.pos = mul(view_proj, float4(world, 0.0, 1.0));
    o.uv = input.uv;
    return o;
}
