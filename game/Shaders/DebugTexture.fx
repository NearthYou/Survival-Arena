#include "00. Global.fx"
#include "00. Light.fx"

SamplerState samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEXCOORD;
};

VertexOut VS(VertexTextureNormalTangent vin)
{
    VertexOut vout;
	
    vout.PosH = mul(vin.position, W);
    vout.PosH = mul(vout.PosH, VP);
    vout.Tex = vin.uv;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    return DiffuseMap.Sample(samLinear, pin.Tex);
}

float4 PS(VertexOut pin, uniform int index) : SV_Target
{
    float4 c = DiffuseMap.Sample(samLinear, pin.Tex).r;

    // draw as grayscale
    return float4(c.rrr, 1);
}

//technique11 ViewArgbTech
//{
//	pass P0
//	{
//		SetVertexShader(CompileShader(vs_5_0, VS()));
//		SetGeometryShader(NULL);
//		SetPixelShader(CompileShader(ps_5_0, PS()));
//	}
//}

technique11 ViewRedTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(0)));
    }
}

technique11 ViewGreenTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(1)));
    }
}

technique11 ViewBlueTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(2)));
    }
}

technique11 ViewAlphaTech
{
    pass P0
    {
        SetVertexShader(CompileShader(vs_5_0, VS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, PS(3)));
    }
}