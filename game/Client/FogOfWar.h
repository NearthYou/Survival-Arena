#pragma once
#include "MonoBehaviour.h"
#include "IFogOfWar.h"

// FogOfWar.h에 추가
enum class ObjectVisibility {
	FullyVisible,    // 완전히 보임
	PartiallyVisible, // 부분적으로 보임 (경계 영역)
	Hidden          // 완전히 숨김
};

// 셰이더 상수 구조체
struct FogOfWarConstantData {
	Vec3 playerWorldPos;
	float sightRange;
	float darkness;
	float fadeDistance;
	float smoothness;
	float time; // 애니메이션용
	Vec2 padding;
};

//전장의 안개 구현. 
class FogOfWar : public MonoBehaviour, public IFogOfWar
{
	using Super = MonoBehaviour;

public:
	FogOfWar();
	virtual ~FogOfWar();

	virtual void Init() override;
	virtual void Start() override;
	virtual void Update() override;

	// 핵심 가시성 함수들
	virtual bool ShouldRenderObject(shared_ptr<GameObject> _object) override;
	virtual void UpdateFOWSystem() override;

	void SetSightRange(float _range) { m_sightRange = _range; m_needsUpdate = true; }
	void SetDarkness(float _darkness) { m_darkness = _darkness; m_needsUpdate = true; }
	void SetFadeDiatance(float _fade) { m_fadeDistance = _fade; m_needsUpdate = true; }
	void SetSmoothness(float _smoothness) { m_smoothness = _smoothness; }

	bool IsFOWShader(shared_ptr<Shader> _shader);
	void UpdateShadersWithFOWData(FogOfWarData& _fowData);
private:
	void UpdateFOWShader();
	bool IsMapObject(shared_ptr<GameObject> _object);

private:
	//원 설정.
	float m_sightRange = 12.f;
	float m_darkness = 0.7f;
	float m_fadeDistance = 3.0f;
	float m_smoothness = 3.0f;

	float m_curTime = 0.06f;
	float m_updateTime = 0.05f;

	FogOfWarData m_lastFowData = {};
	bool m_isFirstUpdate = true;
	bool m_needsUpdate = true;
};

