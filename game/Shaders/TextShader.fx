#ifndef _TEXT_FX_
#define _TEXT_FX_

#include "00. Global.fx"

////////////////
// Structures //
////////////////

// 개별 클리핑 변수 (Global.fx의 ClippingBuffer 대신)
float4 ClippingRect2 : CLIPRECT = float4(0, 0, 1366, 768);
bool EnableClipping2 : ENABLECLIP = false;


struct VertexInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct PixelInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
    float4 screenPos : TEXCOORD1; // 스크린 좌표 추가
};

////////////////
// Resources  //
////////////////

Texture2D DiffuseMap;

cbuffer TextMaterialBuffer
{
    float4 TextColor;
    float4 OutlineColor;
    float4 BackgroundColor;
    float TextAlpha;
    float OutlineWidth;
    float2 TextPadding;
};

////////////////
// Vertex Shader //
////////////////

PixelInput VS_Text(VertexInput input)
{
    PixelInput output;
    
    // 일반 Transform 사용 (W 행렬)
    float4 worldPos = mul(input.position, W);
    
    // View-Projection 변환
    output.position = mul(worldPos, VP);
    output.screenPos = output.position; // 스크린 좌표 저장
    
    // UV 좌표 전달
    output.uv = input.uv;
    
    // 텍스트 색상 전달
    output.color = TextColor;
    output.color.a *= TextAlpha;
    
    return output;
}

////////////////
// Pixel Shader //
////////////////

float4 PS_OutlineText(PixelInput input) : SV_TARGET
{
    float2 uv = input.uv;
    
    // 텍스처에서 원본 색상 샘플링
    float4 texColor = DiffuseMap.Sample(LinearSampler, uv);
    
    // 텍셀 크기를 동적으로 계산
    float2 texelSize;
    DiffuseMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = (OutlineWidth / texelSize);
    
    // 8방향 외곽선 샘플링
    float outline = 0.0f;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(-texelSize.x, -texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(0.0f, -texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(texelSize.x, -texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(-texelSize.x, 0.0f)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(texelSize.x, 0.0f)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(-texelSize.x, texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(0.0f, texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(texelSize.x, texelSize.y)).a;
    outline /= 8.0f;
    
    // 최종 색상 결정
    float4 finalColor;
    
    if (texColor.a > 0.5f)
    {
        // 텍스트 내부: 지정된 텍스트 색상 사용
        finalColor = input.color;
        finalColor.a *= texColor.a;
    }
    else if (outline > 0.1f)
    {
        // 외곽선 영역: 외곽선 색상 사용
        finalColor = OutlineColor;
        finalColor.a *= outline * TextAlpha;
    }
    else
    {
        discard;
    }
    
    return finalColor;
}

// 클리핑이 적용된 픽셀 셰이더
float4 PS_OutlineText_Clipped(PixelInput input) : SV_TARGET
{
    // 스크린 좌표로 변환
    float2 screenPos = input.screenPos.xy / input.screenPos.w;
    screenPos = screenPos * 0.5f + 0.5f; // [-1,1] -> [0,1]
    screenPos.y = 1.0f - screenPos.y; // Y축 뒤집기
    
    // 뷰포트 크기로 스케일링 (실제 화면 크기에 맞게 조정 필요)
    screenPos.x *= 1366.0f; // 실제 화면 너비
    screenPos.y *= 768.0f; // 실제 화면 높이
    
    // 클리핑 영역 체크
    if (EnableClipping2)
    {
        if (screenPos.x < ClippingRect2.x || screenPos.x > ClippingRect2.z ||
            screenPos.y < ClippingRect2.y || screenPos.y > ClippingRect2.w)
        {
            discard; // 클리핑 영역 밖의 픽셀은 버림
        }
    }
    
    // 기존 텍스트 렌더링 로직
    float2 uv = input.uv;
    
    // 텍스처에서 원본 색상 샘플링
    float4 texColor = DiffuseMap.Sample(LinearSampler, uv);
    
    // 텍셀 크기를 동적으로 계산
    float2 texelSize;
    DiffuseMap.GetDimensions(texelSize.x, texelSize.y);
    texelSize = (OutlineWidth / texelSize);
    
    // 8방향 외곽선 샘플링
    float outline = 0.0f;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(-texelSize.x, -texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(0.0f, -texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(texelSize.x, -texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(-texelSize.x, 0.0f)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(texelSize.x, 0.0f)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(-texelSize.x, texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(0.0f, texelSize.y)).a;
    outline += DiffuseMap.Sample(LinearSampler, uv + float2(texelSize.x, texelSize.y)).a;
    outline /= 8.0f;
    
    // 최종 색상 결정
    float4 finalColor;
    
    if (texColor.a > 0.5f)
    {
        // 텍스트 내부: 지정된 텍스트 색상 사용
        finalColor = input.color;
        finalColor.a *= texColor.a;
    }
    else if (outline > 0.1f)
    {
        // 외곽선 영역: 외곽선 색상 사용
        finalColor = OutlineColor;
        finalColor.a *= outline * TextAlpha;
    }
    else
    {
        discard;
    }
    
    return finalColor;
}

////////////////
// Technique //
////////////////

technique11 TextTech
{
    pass P0
    {
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS_Text()));
        SetPixelShader(CompileShader(ps_5_0, PS_OutlineText()));
    }
// 클리핑 패스 추가
    pass P1
    {
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS_Text()));
        SetPixelShader(CompileShader(ps_5_0, PS_OutlineText_Clipped()));
    }
}

#endif
