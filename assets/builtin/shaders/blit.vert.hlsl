struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOutput main(uint vid : SV_VertexID)
{
    VSOutput o;
    o.uv = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(o.uv * 2.0 - 1.0, 0.0, 1.0);
    return o;
}
