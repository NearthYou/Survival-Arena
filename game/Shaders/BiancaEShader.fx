#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"


cbuffer SnowBuffer
{
    float4 Color;
    float3 Velocity;
    float DrawDistance;

    float3 Origin;
    float Turbulence;

    float3 Extent;
    float Time;
    float SpiralCoef;
    float3 snowPadding;
};

struct VertexInput
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float2 scale : SCALE;
    float2 random : RANDOM;
};

struct V_OUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float alpha : ALPHA;
}; 


V_OUT VS(VertexInput input)
{
    V_OUT output;

    float individualTime = Time + input.random.y * 6.28f;
    
    float maxRadius = length(Extent);
    float currentRadius = maxRadius - (individualTime * 8.f);
    
    float theta = input.random.x * 6.28318f;
    float phi = input.random.y * 3.14159f;
    
    //나선형 입자 모이기.
    float spiralFactor = individualTime * SpiralCoef;
    theta += spiralFactor;
    phi += spiralFactor * 0.5f;
    
    if (currentRadius < 0.f)
    {
        currentRadius = maxRadius + fmod(currentRadius, maxRadius);
    }
    
    input.position.x = Origin.x + currentRadius * sin(phi) * cos(theta);
    input.position.y = Origin.y + currentRadius * cos(phi);
    input.position.z = Origin.z + currentRadius * sin(phi) * sin(theta);
    
    // 이 밑은 world변환등 그리고 Billboard쪽. 
    float4 position = mul(input.position, W);
    float4 worldOrigin = mul(float4(Origin, 1.f), W);
    
    float3 targetDir = normalize(worldOrigin.xyz - position.xyz);
    
    // 월드 업 벡터
    float3 worldUp = float3(0, 1, 0);
    
    // targetDir과 worldUp의 외적
    float3 right = normalize(cross(worldUp, targetDir));
    
    // targetDir과 right의 외적
    float3 up = normalize(cross(targetDir, right));
    
    float3x3 rotMatrix = float3x3(
        right.x, up.x, targetDir.x,
        right.y, up.y, targetDir.y,
        right.z, up.z, targetDir.z
    );
    
    float3 localPos = float3(
        (input.uv.x - 0.5f) * input.scale.x, // 여전히 가로는 right 방향
        0.0f, // Y 오프셋을 0으로
        (1.0f - input.uv.y - 0.5f) * input.scale.y  // 세로를 forward 방향으로
    );
    
    // 회전 행렬로 기울이기
    float3 rotatedPos = mul(localPos, rotMatrix);
    
    position.xyz += rotatedPos;
    position.w = 1.0f;
    output.position = mul(mul(position, V), P);
    output.uv = input.uv;
    output.alpha = 1.0f;

    float4 view = mul(position, V);
    float centerDistance = currentRadius / maxRadius;
    output.alpha = saturate(centerDistance * 0.8f + 0.2f);

    return output;
}

float4 PS(V_OUT input) : SV_Target
{
    
    float2 center = float2(0.5, 0.5);
    float2 uv = input.uv - center;
    
    float dist = length(uv);
    
    float radius = 0.5;
    float borderThickness = 0.08;
    
    float outerEdge = radius;
    float innerEdge = radius - borderThickness;
    
    float borderAlpha = smoothstep(outerEdge + 0.02, outerEdge, dist) -
                       smoothstep(innerEdge + 0.02, innerEdge, dist);
    
    
    float particleAlpha = 1.f - smoothstep(0.4, 0.5, dist);
    float3 borderColor = float3(1.2, 0.4, 0.3);
    
    float3 particleColor = float3(1, 1, 0);
    
    float3 finalColor = particleColor * particleAlpha + borderColor * borderAlpha;
    float finalAlpha = max(particleAlpha * 0.3, borderAlpha * 0.8) * input.alpha;
    
    return float4(finalColor, finalAlpha);
}

technique11 T0
{
    PASS_BS_VP(P0, AlphaBlend, VS, PS)
};