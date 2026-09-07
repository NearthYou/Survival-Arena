#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"
#include "00. GBuffer.fx"

// 데칼 전용 버퍼만 새로 추가 (기존 아웃라인 패턴과 동일)
cbuffer DecalBuffer
{
    matrix DecalMatrix;
    matrix InvDecalMatrix;
    float4 DecalColor;
    float DecalAlpha;
    float3 DecalPadding;
};

// 데칼 텍스처들
Texture2D DecalTexture;
Texture2D DepthTexture;

DepthStencilState DecalDepth
{
    DepthEnable = true;
    DepthFunc = LESS;
    DepthWriteMask = ZERO;
};

MeshOutput VS_Decal(VertexMesh input)
{
    MeshOutput output;
    
    // 기존 월드 변환 방식 재활용 (W 행렬 사용)
    float4 worldPos = mul(input.position, input.world);
    output.position = worldPos;
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.position.z -= 0.00001f;
    
    output.uv = input.uv;
    output.normal = input.normal;
    // shadowPosH는 데칼에서 사용하지 않지만 구조체 호환성을 위해 설정
    output.shadowPosH = float4(0, 0, 0, 0);
    
    return output;
}


// 더 간단한 버전
GBufferOutput PS_Decal(MeshOutput input) : SV_Target
{
    GBufferOutput output;
    //return float4(0, 0, 1, 0.6);
    // 현재 데칼 위치를 데칼 로컬 공간으로 변환
    float3 decalPos = mul(float4(input.worldPosition, 1.0), InvDecalMatrix).xyz;
    
    // 데칼 박스 경계 체크
    clip(0.5 - abs(decalPos.xyz));
    //return float4(1, 1, 1, 1);
    // UV 좌표 계산
    float2 decalUV = decalPos.xz + 0.5;
    
    // 텍스처 샘플링
    float4 decalColor = DecalTexture.Sample(LinearSampler, decalUV);
    decalColor *= DecalColor;
    
    if (decalColor.a < 0.1f)
        discard;
    
    
    output.albedo = decalColor;
    output.normal = float4(normalize(input.normal), 1.0f);
    output.position = float4(input.worldPosition, input.position.z);
    output.material = float4(1, 1, 1, 1); // 기본 머티리얼 속성
    
    
    return output;
}

// 기존 technique 명명 패턴 재활용

technique11 
{
    pass P0
    {
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFFFFFFFF);
        SetDepthStencilState(DecalDepth, 0);
        SetVertexShader(CompileShader(vs_5_0, VS_Decal()));
        SetPixelShader(CompileShader(ps_5_0, PS_Decal()));
    }
}
technique11 T0
{
    pass P0
    {
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFFFFFFFF);
        SetDepthStencilState(DecalDepth, 0);
        SetVertexShader(CompileShader(vs_5_0, VS_Decal()));
        SetPixelShader(CompileShader(ps_5_0, PS_Decal()));
    }
}
