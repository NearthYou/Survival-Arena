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

struct PointLight
{
    float4 positionAndRadius;
    float4 colorAndIntensity;
};

cbuffer LightingConstants : register(b2)
{
    PointLight PointLights[32];
    uint PointLightCount;
    float3 LightingPadding;
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
    float3 worldPosition : TEXCOORD1;
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
    output.worldPosition = mul(localPosition, World).xyz;
    return output;
}

float4 PSMain(VertexOutput input) : SV_TARGET
{
    float4 albedo = BaseColor;
    if (HasTexture != 0)
    {
        albedo *= BaseColorTexture.Sample(LinearSampler, input.texcoord);
    }

    const float3 normal = normalize(input.normal);
    const float3 sunDirection = normalize(float3(-0.45F, 0.8F, -0.35F));
    float3 lighting = 0.20F + 0.55F * saturate(dot(normal, sunDirection));
    [loop]
    for (uint lightIndex = 0; lightIndex < PointLightCount; ++lightIndex)
    {
        const PointLight light = PointLights[lightIndex];
        const float3 toLight = light.positionAndRadius.xyz - input.worldPosition;
        const float distanceSquared = max(dot(toLight, toLight), 0.0001F);
        const float distanceToLight = sqrt(distanceSquared);
        const float attenuationBase = saturate(
            1.0F - distanceToLight / light.positionAndRadius.w);
        const float attenuation = attenuationBase * attenuationBase;
        const float diffuse = saturate(dot(normal, toLight / distanceToLight));
        lighting += light.colorAndIntensity.rgb
            * light.colorAndIntensity.w
            * diffuse
            * attenuation;
    }
    return float4(albedo.rgb * lighting, albedo.a);
}
