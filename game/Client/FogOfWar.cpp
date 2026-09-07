#include "pch.h"
#include "FogOfWar.h"
#include "GameObject.h"
#include "Transform.h"
#include "Scene.h"
#include "QuadTree.h"
#include "Renderer.h"
#include "Material.h"
#include "HealthBar.h"

FogOfWar::FogOfWar()
{
}

FogOfWar::~FogOfWar()
{
}

void FogOfWar::Init()
{
	Super::Init();

}

void FogOfWar::Start()
{
	//UpdateFOWShader();
}

void FogOfWar::Update()
{

	Super::Update();

	m_curTime += DT;

	if (m_curTime > m_updateTime) {
		m_curTime = 0.f;
		m_needsUpdate = true;
		UpdateFOWSystem();
	}
}

bool FogOfWar::ShouldRenderObject(shared_ptr<GameObject> _object)
{
	if (!_object) return false;

	if (IsMapObject(_object))return true;
	if (_object == GetGameObject()) return true;
	if (_object->GetType() == OBJECTTYPE::ITEMBOX) return true;

	Vec3 playerPos = GetTransform()->GetPosition();
	Vec3 objPos = _object->GetTransform()->GetPosition();

	float distance = Vec3::Distance(playerPos, objPos);

	return distance <= m_sightRange;
}

void FogOfWar::UpdateFOWSystem()
{
	UpdateFOWShader();
}

bool FogOfWar::IsFOWShader(shared_ptr<Shader> _shader)
{
	return _shader->IsFOWShader();
}

void FogOfWar::UpdateShadersWithFOWData(FogOfWarData& _fowData)
{

	const auto& objects = CURSCENE->GetQuadTree()->GetInsertedObject();
	shared_ptr<Camera> camera = CURSCENE->GetMainCamera()->GetCamera();

	Vec3 playerPos = _fowData.playerWorldPos;
	float maxRange = _fowData.sightRange * 1.2f;

	for (auto& obj : objects) {
		if (CURSCENE->GetQuadTree()->IsObjectVisible(obj, camera)) {
			Vec3 objPos = obj->GetTransform()->GetPosition();
			float distance = Vec3::Distance(playerPos, objPos);

			auto healthBar = obj->GetComponent<HealthBar>();

			if (distance <= maxRange) {
				RENDER->SetFOWData(_fowData);
			}

			if (distance < maxRange - 2.6f) {
				if (healthBar != nullptr && obj->GetType() == OBJECTTYPE::MONSTER) {
					healthBar->SetVisible(true);
				}
				else if (healthBar != nullptr && obj->GetType() == OBJECTTYPE::DIEMONSTER) {
					healthBar->SetVisible(false);
				}
			}
			else {
				if (healthBar != nullptr) {
					healthBar->SetVisible(false);
				}
			}
			
		}
	}
}

void FogOfWar::UpdateFOWShader()
{
	FogOfWarData fowData = {};
	fowData.playerWorldPos = GetTransform()->GetPosition();
	fowData.sightRange = m_sightRange;
	fowData.darkness = m_darkness;
	fowData.fadeDistance = m_fadeDistance;
	fowData.smoothness = m_smoothness;
	fowData.time = GetTickCount64() / 1000.f;

	if (!m_isFirstUpdate && memcmp(&m_lastFowData, &fowData, sizeof(FogOfWarData)) == 0)
		return;

	m_lastFowData = fowData;
	m_isFirstUpdate = false;


	UpdateShadersWithFOWData(fowData);
}


bool FogOfWar::IsMapObject(shared_ptr<GameObject> _object)
{
	if (_object->GetType() == OBJECTTYPE::MAP) return true;
	return false;
}
