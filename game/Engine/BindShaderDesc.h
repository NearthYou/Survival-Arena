#pragma once
#include "ConstantBuffer.h"

struct GlobalDesc {
	Matrix V = Matrix::Identity;
	Matrix P = Matrix::Identity;
	Matrix VP = Matrix::Identity;
	Matrix Vinv = Matrix::Identity;
	Vec3 CamPos = Vec3(0, 0, 0);
	float padding;
};

struct TransformDesc {
	Matrix W = Matrix::Identity;
};

// Light
struct LightDesc
{
	Color ambient = Color(1.f, 1.f, 1.f, 1.f);
	Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
	Color specular = Color(1.f, 1.f, 1.f, 1.f);
	Color emissive = Color(1.f, 1.f, 1.f, 1.f);

	Vec3 direction;
	float padding0;
};

struct MaterialDesc
{
	Color ambient = Color(0.f, 0.f, 0.f, 1.f);
	Color diffuse = Color(1.f, 1.f, 1.f, 1.f);
	Color specular = Color(0.f, 0.f, 0.f, 1.f);
	Color emissive = Color(0.f, 0.f, 0.f, 1.f);
};

//Bone
#define MAX_BONE_TRANSFORMS 250
#define MAX_MODEL_KEYFRAMES 300
#define MAX_MODEL_INSTANCE 250

struct BoneDesc {
	Matrix transforms[MAX_BONE_TRANSFORMS];
};

//Animation
struct KeyframeDesc {
	int32 m_animIndex = 0;
	uint32 m_currFrame = 0;

	//TODO
	uint32 m_nextFrame = 0;
	float m_ratio = 0.f;
	float m_sumTime = 0.f;
	float m_speed = 1.f;
	Vec2 padding;
};

struct TweenDesc {
	TweenDesc() {
		m_curr.m_animIndex = 0;
		m_next.m_animIndex = -1;
	}

	void ClearNextAnim() {
		m_next.m_animIndex = -1;
		m_next.m_currFrame = 0;
		m_next.m_nextFrame = 0;
		m_next.m_sumTime = 0;
		m_tweenSumTime = 0;
		m_tweenRatio = 0;
	}

	float m_tweenDuration = 1.0f;
	float m_tweenRatio = 0.f;
	float m_tweenSumTime = 0.f;
	float padding = 0.f;
	KeyframeDesc m_curr;
	KeyframeDesc m_next;
};

struct InstancedTweenDesc {
	TweenDesc tweens[MAX_MODEL_INSTANCE];
};

struct SnowBillboardDesc {
	Color m_color = Color(1, 1, 1, 1);

	Vec3 m_velocity = Vec3(0, -15, 0);
	float m_drawDistance = 0;

	Vec3 m_origin = Vec3(0, 0, 0);
	float m_turbulence = 5;

	Vec3 m_extent = Vec3(0, 0, 0);
	float m_time = 0;

	float spiralCoef = 1.2f;
	Vec3 padding;
};

struct ParticleDesc {
	Vec3 emitPosW = Vec3(0, 0, 0);
	float timeStep = 0.f;
	Vec3 emitDirW = Vec3(0, 0, 0);
	float gameTime = 0.f;
};

struct TextDesc {
	Vec4 textColor;      // 16바이트
	Vec4 outlineColor;   // 16바이트 
	Vec4 backgroundColor;  // 배경색 추가
	float textAlpha;     // 4바이트
	float outlineWidth;  // 4바이트
	Vec2 textPadding;    // 8바이트 (총 48바이트, 16바이트 정렬)
};

struct FogOfWarData {
	Vec3 playerWorldPos;
	float sightRange;
	float darkness;
	float fadeDistance;
	float smoothness;
	float time;
};

#define MAX_LIGHTS 20
struct MultiLightDesc {
	LightDesc lights[MAX_LIGHTS];
	int activeLightCount;
	float padding[3];
};

// 풀스크린 쿼드 버텍스 데이터
struct QuadVertex
{
	Vec3 position;
	Vec2 uv;
};


struct OutlineDesc {
	int objType;
	int padding[3];
};

struct DecalBufferData {
	Matrix decalMatrix;
	Matrix invDecalMatrix;
	Vec4 decalColor;
	float decalAlpha;
	Vec3 padding;
};

// ScrollView 클리핑 전용 버퍼
struct ScrollViewClippingData
{
	Vec4 clippingRect;
	float enableClipping;
	Vec3 padding;
};

struct HealthBarData {
	float healthRatio;
	float manaRatio;
	int type;
	int padding;
};