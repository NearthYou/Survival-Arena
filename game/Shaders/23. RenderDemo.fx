#include "00. Global.fx"
#include "00. Light.fx"
#include "00. Render.fx"


float4 PS(MeshOutput input) : SV_TARGET
{
    //float4 color = ComputeLight(input.normal, input.uv, input.worldPosition);
    float shadow = CalcShadowFactor(ShadowMap, input.shadowPosH);
    
    float4 color = ComputeLight(input.normal, input.uv, input.worldPosition, shadow);
    //float4 color = DiffuseMap.Sample(LinearSampler, input.uv);

    return color;
}

float4 PS_RED(MeshOutput input) : SV_Target
{
    return float4(1, 0, 0, 1);
}

//GPU와 CPU를 둘 다 일 많이 시키는 것. 
//GPU를 많이 쓰자, 인공지능과 암호화폐에서 ComputeShader를 씀.

//GPU한테 어떤 식으로 일을 분배할 것인가??
technique11 T0
{
	//여러 개의 패스를 둔다. 
    PASS_VP(P0, VS_Mesh, PS)
    PASS_VP(P1, VS_Model, PS)
    PASS_VP(P2, VS_Animation, PS)
    PASS_RS_VP(P3, FillModeWireFrame, VS_Mesh, PS_RED)

};

technique11 shadowTech
{
	PASS_SHADOW_V(P0, VS_Mesh)
	PASS_SHADOW_V(P1, VS_Model)
	PASS_SHADOW_V(P2, VS_Animation)
};
