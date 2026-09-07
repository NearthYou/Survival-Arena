#ifndef _OUTLINE_POST_FX_
#define _OUTLINE_POST_FX_

#include "00. Global.fx"

Texture2D gNormalBuffer;
Texture2D gPositionBuffer;
Texture2D gMaterialBuffer;

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


float4 outlineColor = float4(1, 1, 1, 1);

// 풀스크린 쿼드 버텍스 셰이더
VertexQuadOutput VS_Quad(VertexQuad input)
{
    VertexQuadOutput output;
    output.position = input.position;
    output.uv = input.uv;
    return output;
}

float4 PS_OutlinePost(VertexQuadOutput input) : SV_Target
{
    float2 screenSize = float2(1366, 768);
    float2 texelSize = 1.0f / screenSize;
    
    // 기본값: 투명
    float4 result = float4(0, 0, 0, 0);
    
    // Material Buffer에서 ObjectType 확인
    float4 materialData = gMaterialBuffer.Sample(LinearSampler, input.uv);
    float objectType = materialData.r;
    
    // ITEMBOX인 경우만 처리
    if (abs(objectType - 1.0f) <= 0.01f)
    {
        // Sobel 외곽선 검출
        float3 normalSample[9];
        float3 positionSample[9];
        
        int idx = 0;
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                float2 offset = float2(x, y) * texelSize * 0.01f;
                normalSample[idx] = gNormalBuffer.Sample(LinearSampler, input.uv + offset).xyz;
                positionSample[idx] = gPositionBuffer.Sample(LinearSampler, input.uv + offset).xyz;
                idx++;
            }
        }
        
        float normalDiff = 0;
        float positionDiff = 0;
        
        [unroll]
        for (int i = 0; i < 9; i++)
        {
            if (i == 4)
                continue;
            normalDiff += distance(normalSample[4], normalSample[i]);
            positionDiff += distance(positionSample[4], positionSample[i]);
        }
        
        // 외곽선 판정
        if (normalDiff > 0.1f || positionDiff > 0.5f)
        {
            result = outlineColor;
        }
    }
    
    return result; // 항상 값 반환
}

technique11 OutlineTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS_Quad()));
        SetPixelShader(CompileShader(ps_5_0, PS_OutlinePost()));
        SetBlendState(AdditiveBlend, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
    }
}

#endif
