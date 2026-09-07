#ifndef _DEFERRED_LIGHTING_FX_
#define _DEFERRED_LIGHTING_FX_

#include "00. Global.fx"
#include "00. Light.fx"

// G-Buffer 텍스처들
Texture2D GBufferAlbedo : register(t0);
Texture2D GBufferNormal : register(t1);
Texture2D GBufferPosition : register(t2);
Texture2D GBufferMaterial : register(t3);

// FOW 상수 버퍼
cbuffer FogOfWarData : register(b5)
{
    float3 g_playerWorldPos;
    float g_sightRange;
    float g_darkness;
    float g_fadeDistance;
    float g_smoothness;
    float g_time;
}

// 풀스크린 쿼드용 구조체
struct VertexQuad
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VertexQuadOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

// 풀스크린 쿼드 버텍스 셰이더
VertexQuadOutput VS_Quad(VertexQuad input)
{
    VertexQuadOutput output;
    output.position = input.position;
    output.uv = input.uv;
    return output;
}

// FOW 계산 함수
float CalculateFogOfWar(float3 worldPos)
{
    float distance = length(worldPos - g_playerWorldPos);
    
    if (distance > g_sightRange * 1.2f)
    {
        return max(g_darkness, 0.4f);
    }
    
    float fogFactor = 1.0f;
    
    if (distance > g_sightRange)
    {
        fogFactor = max(g_darkness, 0.4f);
    }
    else if (distance > (g_sightRange - g_fadeDistance))
    {
        float fadeRatio = (distance - (g_sightRange - g_fadeDistance)) / g_fadeDistance;
        fadeRatio = smoothstep(0.0f, 1.0f, fadeRatio);
        fadeRatio = pow(fadeRatio, g_smoothness);
        
        fogFactor = lerp(1.0f, max(g_darkness, 0.4f), fadeRatio);
    }
    
    return fogFactor;
}

// 디퍼드 라이팅용 라이팅 계산 함수
float4 ComputeDeferredLight(float4 albedo, float3 normal, float3 worldPos, float shadow)
{
    float4 ambientColor = 0;
    float4 diffuseColor = 0;
    float4 specularColor = 0;
    float4 emissiveColor = 0;

    // Ambient
    //ambientColor = albedo * GlobalLight.ambient * Material.ambient;
    ambientColor = albedo * GlobalLight.ambient * shadow * 3.0f;
    
   
    // Diffuse
    float3 lightDir = -normalize(GlobalLight.direction);
    float NdotL = saturate(dot(normal, lightDir));
    diffuseColor = albedo * GlobalLight.diffuse * Material.diffuse * NdotL;
    
    // Specular
    if (NdotL > 0)
    {
        float3 reflectDir = reflect(-lightDir, normal);
        float3 viewDir = normalize(CamPos - worldPos);
        float spec = pow(saturate(dot(viewDir, reflectDir)), 10);
        specularColor = GlobalLight.specular * Material.specular * spec;
    }
    
    // Emissive
    float3 viewDir = normalize(CamPos - worldPos);
    float viewDotNormal = saturate(dot(viewDir, normal));
    float emissive = 1.0f - viewDotNormal;
    emissive = smoothstep(0.0f, 1.0f, emissive);
    emissive = pow(emissive, 2);
    emissiveColor = GlobalLight.emissive * Material.emissive * emissive;
    
    
    // 최종 색상 계산
    float4 finalColor = ambientColor + (diffuseColor + specularColor) * shadow;
    
    
    // 최소 밝기 보장 (기존 ComputeLight와 동일)
    //finalColor.rgb = max(finalColor.rgb, albedo.rgb * 0.95f);
    
    return finalColor;
}

// 디퍼드 라이팅 픽셀 셰이더
float4 PS_DeferredLightingWithFOW(VertexQuadOutput input) : SV_Target
{
    int2 screenPos = (int2) (input.position.xy);
    
    // G-Buffer에서 데이터 로드
    float4 albedo = GBufferAlbedo.Load(int3(screenPos, 0));
    float4 normalData = GBufferNormal.Load(int3(screenPos, 0));
    float4 positionData = GBufferPosition.Load(int3(screenPos, 0));
   
    
    if (albedo.a < 0.01f)
        discard;
    
    float3 worldPos = positionData.xyz;
    float3 normal = normalize(normalData.xyz);
    
    // 섀도우 계산
    float4 shadowPosH = mul(float4(worldPos, 1.0f), ShadowTransform);
    float shadow = CalcShadowFactor(ShadowMap, shadowPosH);
    
    // 디퍼드 라이팅 계산
    float4 baseColor = ComputeDeferredLight(albedo, normal, worldPos, shadow);
    
    // FOW 계산
    float fogFactor = CalculateFogOfWar(worldPos);
    
    // FOW 효과 적용
    float3 grayColor = dot(baseColor.rgb, float3(0.299, 0.587, 0.114));
    grayColor = grayColor * float3(0.7, 0.7, 0.8);
    
    float grayIntensity = saturate(g_smoothness * 0.5f);
    float3 foggedColor = lerp(baseColor.rgb, grayColor, (1.0f - fogFactor) * grayIntensity);
    
    float minBrightness = max(g_darkness, 0.4f);
    float3 finalColor = foggedColor * max(fogFactor, minBrightness);
    
    // 어둠 영역에 푸른빛 색조 추가
    if (fogFactor < 0.6f)
    {
        float blueTint = (0.6f - fogFactor) * 0.1f;
        finalColor = lerp(finalColor, finalColor * float3(0.9f, 0.95f, 1.05f), blueTint);
    }
    
    //finalColor.rgb = max(finalColor.rgb, albedo.rgb * 1.2f);
    //Outline
    
    
    return float4(finalColor, baseColor.a) * 2.f;
}


// 디퍼드 라이팅 픽셀 디버그
float4 PS_DeferredDEBUG(VertexQuadOutput input) : SV_Target
{
    return float4(1, 0, 0, 1);
}

float4 PS_DebugGBuffer(VertexQuadOutput IN) : SV_Target
{
    float4 a = GBufferAlbedo.Sample(LinearSampler, IN.uv);
    float4 n = GBufferNormal.Sample(LinearSampler, IN.uv);
    float4 p = GBufferPosition.Sample(LinearSampler, IN.uv);
    float4 m = GBufferMaterial.Sample(LinearSampler, IN.uv);

    // 좌측 25% 알베도, 다음 25% 노말, 등 분할
    if (IN.uv.x < 0.25)
        return (a.r == 0 ? float4(1, 0, 1, 1) : a);
    else if (IN.uv.x < 0.5)
        return (n.r == 0 ? float4(0, 1, 1, 1) : n);
    else if (IN.uv.x < 0.75)
        return (p.r == 0 ? float4(1, 1, 0, 1) : p);
    else
        return (m.r == 0 ? float4(1, 0.5, 0, 1) : m);
}
#endif