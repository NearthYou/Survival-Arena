cbuffer TransparentConstants : register(b0)
{
    row_major float4x4 ViewProjection;
};

struct InstancedVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    uint4 jointIndices : BLENDINDICES0;
    float4 jointWeights : BLENDWEIGHT0;
    float4 worldRow0 : INSTANCEWORLD0;
    float4 worldRow1 : INSTANCEWORLD1;
    float4 worldRow2 : INSTANCEWORLD2;
    float4 worldRow3 : INSTANCEWORLD3;
};

float4 VSMain(InstancedVertexInput input) : SV_POSITION
{
    const row_major float4x4 instanceWorld = float4x4(
        input.worldRow0,
        input.worldRow1,
        input.worldRow2,
        input.worldRow3);
    return mul(float4(input.position, 1.0F), mul(instanceWorld, ViewProjection));
}

float4 PSMain() : SV_TARGET
{
    return float4(0.10F, 0.75F, 1.0F, 0.38F);
}
