cbuffer SceneConstants : register(b0)
{
    row_major float4x4 WorldViewProjection;
    row_major float4x4 World;
    float4 BaseColor;
    uint HasTexture;
    float Roughness;
    float2 ScenePadding;
};

cbuffer SkinConstants : register(b1)
{
    row_major float4x4 BoneMatrices[64];
};

Texture2D BaseColorTexture : register(t0);
SamplerState MaterialSampler : register(s0);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    uint4 jointIndices : BLENDINDICES0;
    float4 jointWeights : BLENDWEIGHT0;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct GBufferOutput
{
    float4 albedoRoughness : SV_TARGET0;
    float2 octNormal : SV_TARGET1;
};

float2 EncodeOctahedralNormal(float3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
    if (normal.z < 0.0F)
    {
        const float2 signs = float2(
            normal.x >= 0.0F ? 1.0F : -1.0F,
            normal.y >= 0.0F ? 1.0F : -1.0F);
        normal.xy = (1.0F - abs(normal.yx)) * signs;
    }
    return normal.xy;
}

VertexOutput VSMain(VertexInput input)
{
    float4 localPosition = float4(input.position, 1.0F);
    float3 localNormal = input.normal;
    const float weightSum = dot(input.jointWeights, 1.0F);
    if (weightSum > 0.0001F)
    {
        localPosition = 0.0F;
        localNormal = 0.0F;
        [unroll]
        for (uint influence = 0; influence < 4; ++influence)
        {
            const float weight = input.jointWeights[influence];
            localPosition += mul(
                float4(input.position, 1.0F),
                BoneMatrices[input.jointIndices[influence]]) * weight;
            localNormal += mul(
                float4(input.normal, 0.0F),
                BoneMatrices[input.jointIndices[influence]]).xyz * weight;
        }
    }

    VertexOutput output;
    output.position = mul(localPosition, WorldViewProjection);
    output.normal = normalize(mul(float4(localNormal, 0.0F), World).xyz);
    output.texcoord = input.texcoord;
    return output;
}

GBufferOutput PSMain(VertexOutput input)
{
    float4 albedo = BaseColor;
    if (HasTexture != 0)
    {
        albedo *= BaseColorTexture.Sample(MaterialSampler, input.texcoord);
    }

    GBufferOutput output;
    output.albedoRoughness = float4(albedo.rgb, Roughness);
    output.octNormal = EncodeOctahedralNormal(normalize(input.normal));
    return output;
}
