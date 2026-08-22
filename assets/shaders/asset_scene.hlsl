cbuffer SceneConstants : register(b0)
{
    row_major float4x4 WorldViewProjection;
    row_major float4x4 World;
    float4 BaseColor;
    uint HasTexture;
    float3 ScenePadding;
};

cbuffer SkinConstants : register(b1)
{
    row_major float4x4 BoneMatrices[64];
};

Texture2D BaseColorTexture : register(t0);
SamplerState LinearSampler : register(s0);

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
            localPosition += mul(float4(input.position, 1.0F), BoneMatrices[input.jointIndices[influence]]) * weight;
            localNormal += mul(float4(input.normal, 0.0F), BoneMatrices[input.jointIndices[influence]]).xyz * weight;
        }
    }

    VertexOutput output;
    output.position = mul(localPosition, WorldViewProjection);
    output.normal = normalize(mul(float4(localNormal, 0.0F), World).xyz);
    output.texcoord = input.texcoord;
    return output;
}

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float4 albedo = BaseColor;
    if (HasTexture != 0)
    {
        albedo *= BaseColorTexture.Sample(LinearSampler, input.texcoord);
    }

    const float3 lightDirection = normalize(float3(-0.45F, 0.8F, -0.35F));
    const float lighting = 0.35F + 0.65F * saturate(dot(normalize(input.normal), lightDirection));
    return float4(albedo.rgb * lighting, albedo.a);
}
