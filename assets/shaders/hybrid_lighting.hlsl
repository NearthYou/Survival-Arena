struct PointLight
{
    float4 positionAndRadius;
    float4 colorAndIntensity;
};

cbuffer LightingConstants : register(b0)
{
    row_major float4x4 InverseViewProjection;
    row_major float4x4 LightViewProjection;
    PointLight PointLights[32];
    uint PointLightCount;
    float ShadowTexelSize;
    float2 LightingPadding;
};

Texture2D AlbedoRoughnessTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D DepthTexture : register(t2);
Texture2D ShadowTexture : register(t3);
SamplerState GBufferSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float3 DecodeOctahedralNormal(float2 encoded)
{
    float3 normal = float3(encoded.xy, 1.0F - abs(encoded.x) - abs(encoded.y));
    if (normal.z < 0.0F)
    {
        const float2 signs = float2(
            normal.x >= 0.0F ? 1.0F : -1.0F,
            normal.y >= 0.0F ? 1.0F : -1.0F);
        normal.xy = (1.0F - abs(normal.yx)) * signs;
    }
    return normalize(normal);
}

float SampleDirectionalShadow(float3 worldPosition)
{
    const float4 lightClip = mul(float4(worldPosition, 1.0F), LightViewProjection);
    const float3 projected = lightClip.xyz / lightClip.w;
    const float2 shadowTexcoord = float2(
        projected.x * 0.5F + 0.5F,
        0.5F - projected.y * 0.5F);
    if (projected.z <= 0.0F
        || projected.z >= 1.0F
        || any(shadowTexcoord < 0.0F)
        || any(shadowTexcoord > 1.0F))
    {
        return 1.0F;
    }

    float visibility = 0.0F;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const float2 offset = float2(x, y) * ShadowTexelSize;
            visibility += ShadowTexture.SampleCmpLevelZero(
                ShadowSampler,
                shadowTexcoord + offset,
                projected.z - 0.001F);
        }
    }
    return visibility / 9.0F;
}

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 texcoord = float2((vertexId << 1) & 2, vertexId & 2);
    VertexOutput output;
    output.position = float4(
        texcoord.x * 2.0F - 1.0F,
        1.0F - texcoord.y * 2.0F,
        0.0F,
        1.0F);
    output.texcoord = texcoord;
    return output;
}

float4 PSMain(VertexOutput input) : SV_TARGET
{
    const float depth = DepthTexture.SampleLevel(GBufferSampler, input.texcoord, 0).r;
    if (depth >= 1.0F)
    {
        discard;
    }

    const float4 albedoRoughness = AlbedoRoughnessTexture.SampleLevel(
        GBufferSampler,
        input.texcoord,
        0);
    const float2 encodedNormal = NormalTexture.SampleLevel(
        GBufferSampler,
        input.texcoord,
        0).xy;
    const float3 normal = DecodeOctahedralNormal(encodedNormal);
    const float2 clipPosition = float2(
        input.texcoord.x * 2.0F - 1.0F,
        1.0F - input.texcoord.y * 2.0F);
    float4 worldPosition = mul(
        float4(clipPosition, depth, 1.0F),
        InverseViewProjection);
    worldPosition /= worldPosition.w;

    const float3 sunDirection = normalize(float3(-0.45F, 0.8F, -0.35F));
    const float shadow = SampleDirectionalShadow(worldPosition.xyz);
    float3 lighting = 0.20F
        + 0.55F * saturate(dot(normal, sunDirection)) * shadow;
    [loop]
    for (uint lightIndex = 0; lightIndex < PointLightCount; ++lightIndex)
    {
        const PointLight light = PointLights[lightIndex];
        const float3 toLight = light.positionAndRadius.xyz - worldPosition.xyz;
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
    return float4(albedoRoughness.rgb * lighting, 1.0F);
}
