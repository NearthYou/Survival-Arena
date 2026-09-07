#ifndef _IMAGE_SHADER_FX_
#define _IMAGE_SHADER_FX_

#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"

// 개별 클리핑 변수 (Global.fx의 ClippingBuffer 대신)
float4 ClippingRect2 : CLIPRECT = float4(0, 0, 1366, 768);
bool EnableClipping2 : ENABLECLIP = false;

// ScrollView별 독립적인 상수 버퍼 정의 (슬롯 10 사용)
cbuffer ScrollViewClippingBuffer : register(b10)
{
    float4 ScrollViewClippingRect; // x=left, y=top, z=right, w=bottom
    bool ScrollViewEnableClipping;
    float3 ScrollViewClippingPadding;
};

cbuffer HealthBarBuffer
{
    float HealthRatio;
    float ManaRatio;
    int type; // 0은 일반, 1은 몬스터(MP안보임)
    int HealthPadding;
};

SamplerState ImageSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

// 버텍스 셰이더 입력
struct VertexInput2
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};

// 버텍스 셰이더 출력
struct VertexOutput2
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 screenPos : TEXCOORD1; // 스크린 좌표 추가
};

// 버텍스 셰이더
VertexOutput2 VS(VertexInput2 input)
{
    VertexOutput2 output;
    
    output.position = mul(input.position, W);
    output.position = mul(output.position, VP);
    output.screenPos = output.position; // 스크린 좌표 저장
    output.uv = input.uv;
    
    return output;
}

// 클리핑이 적용된 픽셀 셰이더 수정
float4 PS_Clipped(VertexOutput2 input) : SV_TARGET
{
    // 스크린 좌표로 변환
    float2 screenPos = input.screenPos.xy / input.screenPos.w;
    screenPos = screenPos * 0.5f + 0.5f;
    screenPos.y = 1.0f - screenPos.y;
    
    screenPos.x *= 1366.0f;
    screenPos.y *= 768.0f;
    
    // 개별 상수 버퍼의 클리핑 변수 사용
    if (ScrollViewEnableClipping)
    {
        if (screenPos.x < ScrollViewClippingRect.x || screenPos.x > ScrollViewClippingRect.z ||
            screenPos.y < ScrollViewClippingRect.y || screenPos.y > ScrollViewClippingRect.w)
        {
            discard;
        }
    }
    
    float4 color = DiffuseMap.Sample(ImageSampler, input.uv);
    return color;
}

// 픽셀 셰이더
float4 PS(VertexOutput2 input) : SV_TARGET
{
    float4 color = DiffuseMap.Sample(ImageSampler, input.uv);
   
    //// 알파 테스트 (완전히 투명한 픽셀은 버림)
    //if (color.a < 0.01)
    //    discard;
    
    return color;
}

// 알파 블렌딩을 위한 픽셀 셰이더
float4 PS_Alpha(VertexOutput2 input) : SV_TARGET
{
    float4 color = DiffuseMap.Sample(ImageSampler, input.uv);
   
    // // 알파 테스트 (완전히 투명한 픽셀은 버림)
    //if (color.a < 1)
    //    discard;
    
    return color;
}

// 픽셀 셰이더 (단색 사용 - Material 색상 사용)
float4 PS_SolidColor(VertexOutput2 input) : SV_TARGET
{
    return Material.diffuse; // Material 상수 버퍼의 diffuse 색상 사용
}

// 알파 블렌딩을 위한 픽셀 셰이더
float4 PS_ReplaceColor(VertexOutput2 input) : SV_TARGET
{
    float4 color = DiffuseMap.Sample(ImageSampler, input.uv);
   
    float4 outColor = Material.diffuse;
    outColor.a = color.a;
    
    // // 알파 테스트 (완전히 투명한 픽셀은 버림)
    //if (color.a < 1)
    //    discard;
    
    return outColor;
}

float4 PS_HealthBar(VertexOutput2 input) : SV_TARGET
{ 
    float4 color = DiffuseMap.Sample(ImageSampler, input.uv);
    if (input.uv.x > HealthRatio)
    {
        return float4(0, 0, 0, 1);
    }
    return color;
}


float4 PS_ManaBar(VertexOutput2 input) : SV_TARGET
{ 
    float4 color = DiffuseMap.Sample(ImageSampler, input.uv);
    if (input.uv.x > ManaRatio)
    {
        discard;
    }
    return color;
}
// 테크닉
float4 PS_Tinted(VertexOutput2 input) : SV_TARGET
{
    return DiffuseMap.Sample(ImageSampler, input.uv) * Material.diffuse;
}

technique11 T0
{
    // 기본 패스
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));
    }
    
    // 알파 블렌딩 패스
    pass P1
    {
        SetDepthStencilState(UIDepthStencil, 0); // UI 전용 깊이 스텐실 상태
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));
    }

    // 단색 알파 블렌딩 패스
    pass P2
    {
        SetDepthStencilState(UIDepthStencil, 0);
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_SolidColor()));
    }

    // 단색 패스
    pass P3
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_SolidColor()));
    }

    // 클리핑 패스 추가
    pass P4
    {
        SetDepthStencilState(UIDepthStencil, 0);
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_Clipped()));
    }

    //알파 블렌딩 + 단색으로 설정 패스
    pass P5
    {
        SetDepthStencilState(UIDepthStencil, 0); // UI 전용 깊이 스텐실 상태
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_ReplaceColor()));
    }


    pass P6
    {
        SetDepthStencilState(UIDepthStencil, 0);
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_HealthBar()));
    }

    pass P7
    {
        SetDepthStencilState(UIDepthStencil, 0);
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_ManaBar()));
    }

    // 알파 블렌딩 패스
    pass P8
    {
        
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS()));
    }
    pass P9
    {
        SetDepthStencilState(UIDepthStencil, 0);
        SetBlendState(AlphaBlend, float4(0, 0, 0, 0), 0xFF);
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetPixelShader(CompileShader(ps_5_0, PS_Tinted()));
    }

}

#endif
