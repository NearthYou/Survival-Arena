// 셰이더 파일에 디버그 심볼 추가
#pragma enable_d3d11_debug_symbols

#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"
#include "00. GBuffer.fx"
#include "00. DeferredLighting.fx"



////////////////
// Functions  //
////////////////


////////////////
// Pixel Shaders
////////////////

// 기본 FOW 픽셀 셰이더
float4 PS_FOW(MeshOutput input) : SV_TARGET
{
    
    //float distance = length(input.worldPosition - g_playerWorldPos);
    //if (distance > 25.f)
    //{
    //    discard;
    //}
    
    
    //그림자 계산. 
    float shadow = CalcShadowFactor(ShadowMap, input.shadowPosH);
    // 기존 라이팅 계산
    float4 baseColor = ComputeLight(input.normal, input.uv, input.worldPosition, shadow);
    
    // FOW 계산
    float fogFactor = CalculateFogOfWar(input.worldPosition);
    
    // 회색 필터 효과 계산
    float3 grayColor = dot(baseColor.rgb, float3(0.299, 0.587, 0.114)); // 그레이스케일 변환
    grayColor = grayColor * float3(0.7, 0.7, 0.8); // 약간 푸른빛이 도는 회색
         
    float grayIntensity = saturate(g_smoothness * 0.5f);
    
    // 원본 색상과 회색 사이의 보간 (어두운 영역일수록 회색 강함)
    float3 foggedColor = lerp(baseColor.rgb, grayColor, (1.0f - fogFactor) * grayIntensity);
    
    // 최소 밝기 보장 (g_darkness를 최소 밝기로 재해석)
    float minBrightness = max(g_darkness, 0.4f);
    
    // 최종 색상 계산
    float3 finalColor = foggedColor * max(fogFactor, minBrightness);
    
    // 어둠 영역에 푸른빛 색조 추가 (분위기 연출)
    if (fogFactor < 0.6f)
    {
        float blueTint = (0.6f - fogFactor) * 0.1f;
        finalColor = lerp(finalColor, finalColor * float3(0.9f, 0.95f, 1.05f), blueTint);
    }
    
    return float4(finalColor, baseColor.a);
}

// 텍스처만 사용하는 간단한 FOW 셰이더
float4 PS_FOW_Simple(MeshOutput input) : SV_TARGET
{
    // 텍스처만 샘플링
    float4 baseColor = DiffuseMap.Sample(LinearSampler, input.uv);
    
    // FOW 계산
    float fogFactor = CalculateFogOfWar(input.worldPosition);
    
    // 최종 색상
    float3 finalColor = baseColor.rgb * fogFactor;
    
    return float4(finalColor, baseColor.a);
}

float4 PS_FOW_DEBUG(MeshOutput input) : SV_TARGET
{
    return float4(1, 0, 0, 1); // 간단한 형태
}

float4 PS_FOW_Transparency(MeshOutput input) : SV_TARGET
{
    return float4(0, 0, 0, 0.5);
}

// NavMesh 전용 픽셀 셰이더 추가
float4 PS_NavMesh_Debug(MeshOutput input) : SV_TARGET
{
    return float4(0.0f, 1.0f, 0.0f, 0.5f); // 반투명 녹색
}

float4 PS_NavMesh_Wireframe(MeshOutput input) : SV_TARGET
{
    return float4(0.0f, 0.8f, 0.0f, 1.0f); // 불투명 녹색
}

// G-Buffer 출력용 픽셀 셰이더
float4 ActorTint = float4(1, 1, 1, 1);

GBufferOutput PS_GBuffer(MeshOutput input)
{
    GBufferOutput output;
    
    // 디퓨즈 텍스처 샘플링
    float4 albedo = DiffuseMap.Sample(LinearSampler, input.uv);
    
    if (albedo.a < 0.05f)
        discard;
    
    // G-Buffer 데이터 출력
    output.albedo = float4(albedo.rgb * ActorTint.rgb, albedo.a);
    //output.albedo = float4(0, 0, 0, 1);
    //return output;
    output.normal = float4(normalize(input.normal), 1.0f);
    output.position = float4(input.worldPosition, input.position.z);
    output.material = float4(1, 1, 1, 1); // 기본 머티리얼 속성
    
    // Material.r에 ObjectType 저장
    float objectTypeValue = 0.0f;
    if (objType == 1)
    { // ITEMBOX
        objectTypeValue = 1.0f;
    }
    
    output.material = float4(objectTypeValue, 0.0f, 0.0f, 1.0f);
    
    
    
    // 임시 디버깅: 단계적으로 테스트
    //output.albedo = float4(1, 0, 0, 1); // 빨간색으로 고정
    //output.normal = float4(0, 1, 0, 1); // 초록색으로 고정  
    //output.position = float4(0, 0, 1, 1); // 파란색으로 고정
    //output.material = float4(1, 1, 0, 1); // 노란색으로 고정
    return output;
}


float4 PS_DebugSRV(VertexQuadOutput IN) : SV_Target
{
    // t0만 샘플링
    float4 a = GBufferAlbedo.Sample(LinearSampler, IN.uv);
    
    if (a.r == 0 && a.g == 0 && a.b == 0 && a.a == 0)
    {
        return float4(1, 0, 1, 1);
    }
    return a;
}

////////////////
// Techniques //
////////////////

// 메인 FOW 테크닉
technique11 T0
{
    // 기본 FOW 렌더링
    //PASS_VP(P0, VS_Mesh, PS_FOW)
    //PASS_VP(P1, VS_Model, PS_FOW)
    //PASS_VP(P2, VS_Animation, PS_FOW)
    PASS_VP(P0, VS_Mesh, PS_GBuffer)
    PASS_VP(P1, VS_Model, PS_GBuffer)
    PASS_VP(P2, VS_Animation, PS_GBuffer)
    PASS_RS_VP(P3, FillModeWireFrame, VS_Mesh, PS_FOW_DEBUG)
    PASS_VP(P4, VS_Mesh, PS_FOW_Transparency)
// NavMesh 디버그 렌더링 추가
    PASS_BS_VP(P6, AlphaBlend, VS_Mesh, PS_NavMesh_Debug)
    PASS_RS_VP(P7, FillModeWireFrame, VS_Mesh, PS_NavMesh_Wireframe)

    PASS_VP(P8, VS_Quad, PS_DebugGBuffer)
}

// 그림자 테크닉 (기존과 동일)
technique11 shadowTech
{
    PASS_SHADOW_V(P0, VS_Mesh)
    PASS_SHADOW_V(P1, VS_Model)
    PASS_SHADOW_V(P2, VS_Animation)
}

technique11 DeferredLightingTech
{
    PASS_VP(P0, VS_Quad, PS_DeferredLightingWithFOW)
}

technique11 GBufferTech
{
    PASS_VP(P0, VS_Mesh, PS_GBuffer)
    PASS_VP(P1, VS_Model, PS_GBuffer)
    PASS_VP(P2, VS_Animation, PS_GBuffer)
    PASS_RS_VP(P3, FillModeWireFrame, VS_Mesh, PS_FOW_DEBUG)
    PASS_VP(P4, VS_Mesh, PS_FOW_Transparency)
}
