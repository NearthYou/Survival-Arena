#ifndef _RENDER_FX_
#define _RENDER_FX_


#include "00. Global.fx"
#include "00. Light.fx"

#define MAX_MODEL_TRANSFORMS 250
#define MAX_MODEL_KEYFRAMES 300
#define MAX_MODEL_INSTANCE 250



//Mesh Render

struct VertexMesh
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    
    //INSTANCING
    uint instanceID : SV_INSTANCEID;
    matrix world : INST;
};

MeshOutput VS_Mesh(VertexMesh input)
{
    MeshOutput output;
    
    float4 worldPos = mul(input.position, input.world);
    output.position = worldPos;
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normal = input.normal;
    output.shadowPosH = mul(worldPos, ShadowTransform);
    
    return output;
}

//Model Render

struct VertexModel
{
    float4 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float4 blendIndices : BLEND_INDICES;
    float4 blendWeights : BLEND_WEIGHTS;
    
    //INSTANCING
    uint instanceID : SV_INSTANCEID;
    matrix world : INST;
};

cbuffer BoneBuffer
{
    matrix BoneTransforms[MAX_MODEL_TRANSFORMS];
};

uint BoneIndex;

MeshOutput VS_Model(VertexModel input)
{
    MeshOutput output;
    
    //Model Global로 해서, 모델 내부의 메쉬들 Global좌표로 변경하기. 
    float4 worldPos = mul(input.position, BoneTransforms[BoneIndex]);
	worldPos = mul(worldPos, input.world); // W
	//output.position = mul(input.position, BoneTransforms[BoneIndex]); // Model Global
	//output.position = mul(output.position, input.world); // W
    output.position = worldPos;
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    output.normal = input.normal;
    output.shadowPosH = mul(worldPos, ShadowTransform);
    
    return output;
}


//Animation Render

struct KeyframeDesc
{
    int animIndex;
    uint currFrame;
    uint nextFrame;
    float ratio;
    float sumTime;
    float speed;
    float2 padding;
    
};

struct TweenFrameDesc
{
    float tweenDuration;
    float tweenRatio;
    float tweenSumTime;
    float padding;
    KeyframeDesc curr;
    KeyframeDesc next;
};

cbuffer TweenBuffer
{
    TweenFrameDesc TweenFrames[MAX_MODEL_INSTANCE];
};

Texture2DArray TransformMap;


matrix GetAnimationWorldMatrix(VertexModel input)
{
    float indices[4] = { input.blendIndices.x, input.blendIndices.y, input.blendIndices.z, input.blendIndices.w };
    float weights[4] = { input.blendWeights.x, input.blendWeights.y, input.blendWeights.z, input.blendWeights.w };

    int animIndex[2];
    int currFrame[2];
    int nextFrame[2];
    
    float ratio[2];
    
    animIndex[0] = TweenFrames[input.instanceID].curr.animIndex;
    currFrame[0] = TweenFrames[input.instanceID].curr.currFrame;
    nextFrame[0] = TweenFrames[input.instanceID].curr.nextFrame;
    ratio[0] = TweenFrames[input.instanceID].curr.ratio;
    
    animIndex[1] = TweenFrames[input.instanceID].next.animIndex;
    currFrame[1] = TweenFrames[input.instanceID].next.currFrame;
    nextFrame[1] = TweenFrames[input.instanceID].next.nextFrame;
    ratio[1] = TweenFrames[input.instanceID].next.ratio;
    

    //최대 영향받는 뼈 4개. 
    float4 c0, c1, c2, c3;
    float4 n0, n1, n2, n3;
    
    
    matrix curr = 0;
    matrix next = 0;
    matrix transform = 0;

    for (int i = 0; i < 4; i++)
    {
        c0 = TransformMap.Load(int4(indices[i] * 4 + 0, currFrame[0], animIndex[0], 0));
        c1 = TransformMap.Load(int4(indices[i] * 4 + 1, currFrame[0], animIndex[0], 0));
        c2 = TransformMap.Load(int4(indices[i] * 4 + 2, currFrame[0], animIndex[0], 0));
        c3 = TransformMap.Load(int4(indices[i] * 4 + 3, currFrame[0], animIndex[0], 0));

        
        //그 영향 값을 계산해서 행렬을 계산하고.
        curr = matrix(c0, c1, c2, c3);
        
        n0 = TransformMap.Load(int4(indices[i] * 4 + 0, nextFrame[0], animIndex[0], 0));
        n1 = TransformMap.Load(int4(indices[i] * 4 + 1, nextFrame[0], animIndex[0], 0));
        n2 = TransformMap.Load(int4(indices[i] * 4 + 2, nextFrame[0], animIndex[0], 0));
        n3 = TransformMap.Load(int4(indices[i] * 4 + 3, nextFrame[0], animIndex[0], 0));
        
        //그 영향 값을 계산해서 행렬을 계산하고.
        next = matrix(n0, n1, n2, n3);
        
        matrix result = lerp(curr, next, ratio[0]);
        
        //다음 애니메이션 있는지 체크
        if (animIndex[1] >= 0)
        {
            c0 = TransformMap.Load(int4(indices[i] * 4 + 0, currFrame[1], animIndex[1], 0));
            c1 = TransformMap.Load(int4(indices[i] * 4 + 1, currFrame[1], animIndex[1], 0));
            c2 = TransformMap.Load(int4(indices[i] * 4 + 2, currFrame[1], animIndex[1], 0));
            c3 = TransformMap.Load(int4(indices[i] * 4 + 3, currFrame[1], animIndex[1], 0));
            curr = matrix(c0, c1, c2, c3);

            n0 = TransformMap.Load(int4(indices[i] * 4 + 0, nextFrame[1], animIndex[1], 0));
            n1 = TransformMap.Load(int4(indices[i] * 4 + 1, nextFrame[1], animIndex[1], 0));
            n2 = TransformMap.Load(int4(indices[i] * 4 + 2, nextFrame[1], animIndex[1], 0));
            n3 = TransformMap.Load(int4(indices[i] * 4 + 3, nextFrame[1], animIndex[1], 0));
            next = matrix(n0, n1, n2, n3);

            matrix nextResult = lerp(curr, next, ratio[1]);
            result = lerp(result, nextResult, TweenFrames[input.instanceID].tweenRatio);
        }
        
        //가중치를 곱해서 transform을 결정함. 
        transform += mul(weights[i], result);
    }

    return transform;
}


MeshOutput VS_Animation(VertexModel input)
{
    MeshOutput output;
    
    //Model Global로 해서, 모델 내부의 메쉬들 Global좌표로 변경하기. 
    output.position = mul(input.position, BoneTransforms[BoneIndex]);
    matrix m = GetAnimationWorldMatrix(input);
    
    float4 worldPos = mul(input.position, m);
    worldPos = mul(worldPos, input.world);
    output.position = worldPos;
    
    output.worldPosition = output.position;
    output.position = mul(output.position, VP);
    output.uv = input.uv;
    
    //빛 관련 처리. 
    output.normal = mul(input.normal, (float3x3) input.world);
    output.tangent = mul(input.normal, (float3x3) input.world);
    output.shadowPosH = mul(worldPos, ShadowTransform);
    
    return output;
}

#endif