struct VertexIn
{
    float4 PosH : SV_POSITION;
    float4 Color : COLOR;
};

float4 main(VertexIn vin) : SV_TARGET
{
    return vin.Color;
}