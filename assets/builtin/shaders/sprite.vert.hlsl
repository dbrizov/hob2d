// World-space sprite vertex shader.
//
// Vertex input: unit-quad position in [0,1] x [0,1] and matching UV (top-left origin).
// Per-draw uniform block holds the view-projection (world meters -> NDC) plus the sprite's
// world-space transform: world_pos is the pivot point in meters, size is the quad size in
// meters, pivot is a 0..1 fraction of the quad, and rotation is in radians (world y-up CCW).
//
// The quad is built in world space. Because world is y-up but the texture's UV origin is
// top-left (uv.y increases downward), the local Y offset is negated so the texture stays
// upright after the view-projection (which maps world y-up to the offscreen's y-down NDC).

cbuffer SpriteVS : register(b0, space1)
{
    float4x4 view_proj;
    float2 world_pos;
    float2 size;
    float2 pivot;
    float rotation;
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
