cbuffer FrameConstants : register(b0)
{
    float4x4 worldViewProjection;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR0;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR0;
};

PixelInput VSMain(VertexInput input)
{
    PixelInput output;
    output.position = mul(float4(input.position, 1.0F), worldViewProjection);
    output.color = input.color;
    return output;
}

float4 PSMain(PixelInput input) : SV_TARGET
{
    return float4(input.color, 1.0F);
}

