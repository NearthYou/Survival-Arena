#ifndef _LIGHT_FX_
#define _LIGHT_FX_

#include "00. Global.fx"

////////////////
//   Struct   //
////////////////

struct LightDesc
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 emissive;
    
    float3 direction;
    float padding;
};

struct MaterialDesc
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float4 emissive;
};


////////////////
// ConstBuffer//
////////////////

cbuffer LightBuffer
{
    LightDesc GlobalLight;
};

cbuffer MaterialBuffer
{
    MaterialDesc Material;
};

cbuffer ShadowBuffer
{
    float4x4 ShadowTransform;
};


////////////////
// SRV        //
////////////////

Texture2D DiffuseMap;
Texture2D SpecualrMap;
Texture2D NormalMap;
Texture2D ShadowMap;

////////////////
// Function   //
////////////////

float4 ComputeLight(float3 _normal, float2 _uv, float3 _worldPosition, float shadow = 0)
{
    float4 ambientColor = 0;
    float4 diffuseColor = 0;
    float4 specularColor = 0;
    float4 emissiveColor = 0;
    
    float4 baseTexture = DiffuseMap.Sample(LinearSampler, _uv);
    
    if (baseTexture.a < 0.01f)
        discard;
    
    //Ambient
    {
        float4 color = GlobalLight.ambient * Material.ambient;
        ambientColor = DiffuseMap.Sample(LinearSampler, _uv) * color;
    }
    
    //Diffuse
    {
        float4 color = DiffuseMap.Sample(LinearSampler, _uv);
    
        //내적의 결과물은 스칼라값이 나온다. 
        float value = dot(-GlobalLight.direction, normalize(_normal));
        value = saturate(value);
        diffuseColor = color * value * GlobalLight.diffuse * Material.diffuse;
    }
    
    
    //Specular
    {
        float3 R = reflect(GlobalLight.direction, _normal);
        R = normalize(R);
        float3 cameraPosition = CameraPosition();
        float3 E = normalize(cameraPosition - _worldPosition);
    
        float value = saturate(dot(R, E)); // clamp(0 ~ 1)
        float specular = pow(value, 10);
    
        specularColor = GlobalLight.specular * Material.specular * specular;
    }
    
    
    //Emissive
    {
        float3 cameraPosition = CameraPosition();
        float3 E = normalize(cameraPosition - _worldPosition);
    
        float value = saturate(dot(E, _normal));
        float emissive = 1.0f - value;
    
        //min, max, x (smooth lerp느낌)
        emissive = smoothstep(0.0f, 1.0f, emissive);
        emissive = pow(emissive, 2);
        emissiveColor = GlobalLight.emissive * Material.emissive * emissive;
    }
    
    
    float4 finalColor = ambientColor + (diffuseColor + specularColor + emissiveColor) * shadow;
    
    
    finalColor.rgb = max(finalColor.rgb, baseTexture.rgb * 0.95f);
    
    return finalColor;
}

void ComputeNormalMapping(inout float3 normal, float3 tangent, float2 uv)
{
    // [0, 255]범위에서 [0, 1]로 변환
    float4 map = NormalMap.Sample(LinearSampler, uv);
    
    if (any(map.rgb) == false)
        return;

    float3 N = normalize(normal); //Z
    float3 T = normalize(tangent); //X
    float3 B = normalize(cross(N, T));
    
    //Tangent Space -> World Space
    float3x3 TBN = float3x3(T, B, N);
    
    // [0, 1]범위에서 [-1, 1]범위로 변환
    float3 tangentSpaceNormal = (map.rgb * 2.0f - 1.0f);
    float3 worldNormal = mul(tangentSpaceNormal, TBN);

    
    normal = worldNormal;
}

static const float SMAP_SIZE = 2048.0f;
static const float SMAP_DX = 1.0f / SMAP_SIZE;


SamplerComparisonState samShadow
{
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = BORDER;
    AddressV = BORDER;
    AddressW = BORDER;
    BorderColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    ComparisonFunc = LESS_EQUAL;
};

float CalcShadowFactor(Texture2D shadowMap, float4 shadowPosH)
{
	// Complete projection by doing division by w.
	shadowPosH.xyz /= shadowPosH.w;

	// Depth in NDC space.
    float depth = saturate(shadowPosH.z);

	// Texel size.
    /*
    const float dx = SMAP_DX;
	//return shadowMap.SampleCmpLevelZero(samShadow, shadowPosH.xy, depth).r;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

	[unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
			shadowPosH.xy + offsets[i], depth).r;
    }

    return percentLit /= 9.0f;
    */
    
    //  샘플 9 -> 16개로. 
    const float2 offsets[16] =
    {
        float2(-1.5f, -1.5f), float2(-0.5f, -1.5f), float2(0.5f, -1.5f), float2(1.5f, -1.5f),
        float2(-1.5f, -0.5f), float2(-0.5f, -0.5f), float2(0.5f, -0.5f), float2(1.5f, -0.5f),
        float2(-1.5f, 0.5f), float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(1.5f, 0.5f),
        float2(-1.5f, 1.5f), float2(-0.5f, 1.5f), float2(0.5f, 1.5f), float2(1.5f, 1.5f)
    };
    
    const float dx = SMAP_DX * 1.5f; // 샘플링 범위 축소.
    float percentLit = 0.0f;
    
    [unroll]
    for (int i = 0; i < 16; ++i)
    {
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
            shadowPosH.xy + offsets[i] * dx, depth).r;
    }
    
    float shadowStrength =  percentLit / 16.0f;
    
    float shadowSoftness = 0.5f;
    return lerp(shadowSoftness, 1.f, shadowStrength);

}




#endif