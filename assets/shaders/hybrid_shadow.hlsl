cbuffer ShadowConstants : register(b0)
{
    row_major float4x4 WorldLightViewProjection;
};

cbuffer SkinConstants : register(b1)
{
    row_major float4x4 BoneMatrices[64];
};

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    uint4 jointIndices : BLENDINDICES0;
    float4 jointWeights : BLENDWEIGHT0;
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

float4 VSMain(VertexInput input) : SV_POSITION
{
    float4 localPosition = float4(input.position, 1.0F);
    const float weightSum = dot(input.jointWeights, 1.0F);
    if (weightSum > 0.0001F)
    {
        localPosition = 0.0F;
        [unroll]
        for (uint influence = 0; influence < 4; ++influence)
        {
            localPosition += mul(
                float4(input.position, 1.0F),
                BoneMatrices[input.jointIndices[influence]])
                * input.jointWeights[influence];
        }
    }
    return mul(localPosition, WorldLightViewProjection);
}

float4 VSInstanced(InstancedVertexInput input) : SV_POSITION
{
    const row_major float4x4 instanceWorld = float4x4(
        input.worldRow0,
        input.worldRow1,
        input.worldRow2,
        input.worldRow3);
    return mul(
        float4(input.position, 1.0F),
        mul(instanceWorld, WorldLightViewProjection));
}
