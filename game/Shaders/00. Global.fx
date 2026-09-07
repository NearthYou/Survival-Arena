#ifndef _GLOBAL_FX_
#define _GLOBAL_FX_



Texture1D RandomMap;


////////////////
//Const Buffer//
////////////////

cbuffer GlobalBuffer
{
    matrix V;
    matrix P;
    matrix VP;
    matrix Vinv;
    float3 CamPos; //그림자 그릴 때 라이트가 아닌 카메라 위치가 필요. 
    float padding;
};

cbuffer TransformBuffer
{
    matrix W;
};
//// Global.fx에 추가
//cbuffer ClippingBuffer
//{
//    float4 ClippingRect; // x=left, y=top, z=right, w=bottom (screen coordinates)
//    bool EnableClipping;
//    float3 ClippingPadding;
//};

////////////////
//Vertex Data //
////////////////

struct Vertex
{
    float4 position : POSITION;
};

struct VertexTexture
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
};


struct VertexColor
{
    float4 position : POSITION;
    float4 color : COLOR;
};


struct VertexTextureNormal
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct VertexTextureNormalTangent
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VertexTextureNormalTangentBlend
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;
};
////////////////
//VertexOutput//
////////////////

struct VertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

struct MeshOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION1;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 shadowPosH : TEXCOORD2;
};

////////////////
//SamplerState//
////////////////

SamplerState LinearSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

SamplerState PointSampler
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Wrap;
    AddressV = Wrap;
};

///////////////////
//RasterizerState//
///////////////////

RasterizerState FillModeWireFrame
{
    FillMode = WireFrame;
};

RasterizerState FrontCounterClickwiseTrue
{
    FrontCounterClockwise = true;
};

RasterizerState Depth
{
    DepthBias = 1000;
    DepthBiasClamp = 0.0f;
    SlopeScaledDepthBias = 1.0f;
};

RasterizerState DepthNoCull
{
    DepthBias = 1000;
    DepthBiasClamp = 0.0f;
    SlopeScaledDepthBias = 1.0f;
    CullMode = NONE;
};


BlendState AlphaBlend
{
    AlphaToCoverageEnable = false;

    BlendEnable[0] = true;
    SrcBlend[0] = SRC_ALPHA;
    DestBlend[0] = INV_SRC_ALPHA;
    BlendOp[0] = ADD;

    SrcBlendAlpha[0] = One;
    DestBlendAlpha[0] = Zero;
    BlendOpAlpha[0] = Add;

    RenderTargetWriteMask[0] = 15;
};

BlendState AlphaBlendAlphaToCoverageEnable
{
    AlphaToCoverageEnable = true;

    BlendEnable[0] = true;
    SrcBlend[0] = SRC_ALPHA;
    DestBlend[0] = INV_SRC_ALPHA;
    BlendOp[0] = ADD;

    SrcBlendAlpha[0] = One;
    DestBlendAlpha[0] = Zero;
    BlendOpAlpha[0] = Add;

    RenderTargetWriteMask[0] = 15;
};

BlendState AdditiveBlend
{
    AlphaToCoverageEnable = FALSE;
	BlendEnable[0] = TRUE;
    SrcBlend = SRC_ALPHA;
    DestBlend = ONE;
	BlendOp = ADD;
    SrcBlendAlpha = ZERO;
    DestBlendAlpha = ZERO;
    BlendOpAlpha = ADD;
    RenderTargetWriteMask[0] = 15;
};

BlendState AdditiveBlendAlphaToCoverageEnable
{
    AlphaToCoverageEnable = true;

    BlendEnable[0] = true;
    SrcBlend[0] = One;
    DestBlend[0] = One;
    BlendOp[0] = ADD;

    SrcBlendAlpha[0] = One;
    DestBlendAlpha[0] = Zero;
    BlendOpAlpha[0] = Add;

    RenderTargetWriteMask[0] = 15;
};



///////////////////
//  DepthStencil //
///////////////////

DepthStencilState DisableDepth
{
    DepthEnable = false;
    DepthWriteMask = ZERO;
};

DepthStencilState NoDepthWrites
{
    DepthEnable = true;
    DepthWriteMask = ZERO;
};
// Global.fx에 UI용 DepthStencilState 추가
DepthStencilState UIDepthStencil
{
    DepthEnable = true; // UI는 깊이 테스트 비활성화
    DepthWriteMask = ZERO; // 깊이 쓰기 비활성화
    StencilEnable = true; // 스텐실 사용 안 함
};

///////////////////
//     Macro     //
///////////////////

#define PASS_SHADOW_V(name, vs)						\
pass name											\
{													\
	SetVertexShader(CompileShader(vs_5_0, vs()));	\
	SetPixelShader(NULL);							\
	SetRasterizerState(Depth);						\
}

#define PASS_VP(name, vs, ps)                           \
pass name                                               \
{                                                       \
	SetVertexShader(CompileShader(vs_5_0, vs()));       \
	SetPixelShader(CompileShader(ps_5_0, ps()));        \
}                                 

#define PASS_RS_VP(name, rs, vs, ps)                    \
pass name                                               \
{                                                       \
    SetRasterizerState(rs);                             \
	SetVertexShader(CompileShader(vs_5_0, vs()));       \
	SetPixelShader(CompileShader(ps_5_0, ps()));        \
}       

#define PASS_BS_VP(name, bs, vs, ps)				\
pass name											\
{													\
	SetBlendState(bs, float4(0, 0, 0, 0), 0xFF);	\
    SetVertexShader(CompileShader(vs_5_0, vs()));	\
    SetPixelShader(CompileShader(ps_5_0, ps()));	\
}
///////////////////
//    Function   //
///////////////////


//월드 좌표계에서는 View변환 행렬을 구하면 월드의 역행렬임. 
//회전이 들어가면 Right, Up, Look이 섞여 들어가기 떄문에. 


float3 CameraPosition()
{
    return CamPos;
}


float3 RandUnitVec3(float gameTime, float offset)
{
    float u = (gameTime + offset);
    float v = RandomMap.SampleLevel(LinearSampler, u, 0).xyz;
    
    return normalize(v);
}

#endif