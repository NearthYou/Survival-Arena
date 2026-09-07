#include "pch.h"
#include "BiancaRSkill.h"
#include "BiancaESkillCircle.h"
#include "SnowBillboard.h"
#include "Player.h"
#include "Monster.h"

BiancaRSkill::BiancaRSkill(shared_ptr<Player> _player)
	: Super(_player, 3)
{
	{
		m_skillCooldown = 25.f;
		m_skillName = L"진조의 군림";
		m_skillDesc = L"비앙카가 주문 영창을 하며 자신의 주변에 마법진을 생성합니다.";
		m_curSkillLevel = 0;
		m_maxSkillLevel = 3;
		m_skillImage = RESOURCES->GetOrAddTexture(L"BiancaR", L"..\\Resources\\Textures\\UI\\SkillIcon\\SkillIcon_1042500.png");
	}

	//먼저 바깥 쪽에 생기는 Circle.
	{
		m_outerCircle = make_shared<BiancaESkillCircle>();
		m_outerCircle->AddComponent(make_shared<MeshRenderer>());

		m_outerCircle->SetName(L"Bianca_Outer_Circle");

		m_outerCircle->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
		m_outerCircle->GetMeshRenderer()->SetMesh(RESOURCES->Get<Mesh>(L"Sphere"));
		m_outerCircle->GetMeshRenderer()->GetMaterial()->SetCastShadow(false);
		m_outerCircle->GetTransform()->SetScale(Vec3(8.5f, 0.03f, 8.5f));

		m_outerCircleCollider = make_shared<SphereCollider>();
		m_outerCircle->AddComponent(m_outerCircleCollider);
		m_outerCircleCollider->SetOffsetScale(Vec3(1.f, 50.f, 1.f));
		m_outerCircle->SetActive(false);

		//m_outerCircle->GetTransform()->SetParent(m_playerObject->GetTransform());

		m_outerCircle->GetTransform()->SetLocalPosition(Vec3(0.f, 0.03f, 0.f));
		CURSCENE->Add(m_outerCircle);
	}

	//비앙카 손 위에 생겨날 SnowBillboard.
	{
		auto snowShader = make_shared<Shader>(L"GatherBillboard.fx");
		auto obj = make_shared<GameObject>();
		obj->SetName(L"Bianca_Gather_Blood");
		obj->GetTransform()->SetParent(m_playerObject->GetTransform());
		obj->GetTransform()->SetLocalPosition(Vec3(0, 3, 0));
		obj->AddComponent(make_shared<SnowBillboard>(Vec3(0, 0, 0), Vec3(5, 5, 5), 50));
		{
			// Material
			{
				shared_ptr<Material> material = make_shared<Material>();
				material->SetShader(snowShader);
				//auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\grass.png");
				auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\veigar.jpg");
				material->SetDiffuseMap(texture);
				MaterialDesc& desc = material->GetMaterialDesc();
				desc.ambient = Vec4(1.f);
				desc.diffuse = Vec4(1.f, 0.f, 0.f, 1.f);
				desc.specular = Vec4(1.f);
				RESOURCES->Add(L"Veigar", material);

				obj->GetSnowBillboard()->SetMaterial(material);
				obj->GetSnowBillboard()->SetParticleScale(Vec2(0.6f, 0.4f));
			}
		}
		obj->SetActive(false);
		m_drainBlood = obj;
		CURSCENE->Add(obj);
	}
	//Drain 중앙의 구체. 
	{
		m_drainCircle = make_shared<GameObject>();
		m_drainCircle->SetName(L"Bianca_Drain_Circle");
		m_drainCircle->SetActive(false);
		m_drainCircle->AddComponent(make_shared<MeshRenderer>());
		m_drainCircle->AddComponent(make_shared<SphereCollider>());
		m_drainCircle->GetTransform()->SetParent(m_playerObject->GetTransform());
		m_drainCircle->GetTransform()->SetLocalPosition(Vec3(0.f, 3.5f, 0.f));
		{
			auto mesh = RESOURCES->Get<Mesh>(L"Sphere");
			m_drainCircle->GetMeshRenderer()->SetMesh(mesh);
			m_drainCircle->GetMeshRenderer()->SetPass(0);
			m_drainCircle->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
		}
		CURSCENE->Add(m_drainCircle);
	}
	//그리고 안쪽에서 퍼져나갈 Circle. 
	{
		m_innerCircle = make_shared<BiancaESkillCircle>();
		m_innerCircle->AddComponent(make_shared<MeshRenderer>());

		m_innerCircle->SetName(L"Bianca_Inner_Circle");

		m_innerCircle->GetMeshRenderer()->SetMaterial(RESOURCES->Get<Material>(L"default"));
		m_innerCircle->GetMeshRenderer()->SetMesh(RESOURCES->Get<Mesh>(L"Sphere"));
		m_innerCircle->GetMeshRenderer()->GetMaterial()->SetCastShadow(false);
		m_innerCircle->GetTransform()->SetScale(Vec3(0.1f, 0.03f, 0.1f));

		m_innerCircleCollider = make_shared<SphereCollider>();
		m_innerCircle->AddComponent(m_innerCircleCollider);
		m_innerCircleCollider->SetOffsetScale(Vec3(1.f, 30.f, 1.f));
		m_innerCircle->SetActive(false);
		//m_innerCircle->GetTransform()->SetParent(m_playerObject->GetTransform());
		m_innerCircle->GetTransform()->SetLocalPosition(Vec3(0.f, 0.03f, 0.f));
		CURSCENE->Add(m_innerCircle);
	}
}

BiancaRSkill::~BiancaRSkill()
{
}

void BiancaRSkill::PlaySkill()
{
	if (m_skillcurCooldown > 0.f)
		return;

	if (!skillFlag) {
		skillFlag = true;
		phaseTwo = false;
		m_innerCircle->DamageFlag(true);
		m_outerCircle->DamageFlag(true);
		m_outerCircle->SetActive(true);
		m_drainCircle->SetActive(true);
		m_drainBlood->SetActive(true);
		SOUND->PlaySound(L"Bianca/Bianca_Skill04_Charge.wav", 1, 0.5f);
		SOUND->PlaySound(L"Bianca/Bianca_Skill04_Loop.wav", 1, 0.5f);
		SkillEnd();
		//cout << "Outer Circle전개\n";
	}
}

void BiancaRSkill::Update()
{
	UpdateSkillCoolDown();


	//평소, 캐스팅 중일 때 circle들이 비앙카 따라다님. 
	if (m_circleFollowBianca) {
		Vec3 biancaPosition = m_playerObject->GetTransform()->GetPosition();
		biancaPosition.y += 0.03f;
		m_outerCircle->GetTransform()->SetPosition(biancaPosition);
		m_innerCircle->GetTransform()->SetPosition(biancaPosition);
	}

	if (skillFlag && !phaseTwo)
	{
		if (m_drainEffectDuration > m_drainEffectElapsedTime) {
			m_drainEffectElapsedTime += DT;
		}
		else {
			m_drainBlood->SetActive(false);
			m_drainCircle->SetActive(false);
			phaseTwo = true;
			m_innerCircle->SetActive(true);
			
			m_circleFollowBianca = false;
			//데미지 주기. 
			auto gameObjects = m_outerCircle->GetCollisionObjects();

			for (auto object : gameObjects) {
				if (object->GetType() == OBJECTTYPE::MONSTER) {
					static_pointer_cast<Monster>(object)->Damaged(m_playerObject, m_playerObject->GetStatus().hitAttack * 1.7f);
					SOUND->PlaySound(L"Bianca/Bianca_Skill04_Hit01.wav", 1, 0.5f);
				}
			}
			//cout << "InnerCircle 전개.";
			SOUND->PlaySound(L"Bianca/Bianca_Skill04_Active.wav", 1, 0.5f);
			m_outerCircle->DamageFlag(false);
		}
	}

	if (skillFlag && phaseTwo) 
	{
		if (m_innerCircleDuration > m_innerCircleElapsedTime) {
			m_innerCircleElapsedTime += DT;

			float circleSize = Utils::FLerp(0.f, 8.5f, m_innerCircleElapsedTime / m_innerCircleDuration);
			Vec3 circleScale = Vec3(circleSize, 0.03f, circleSize);

			m_innerCircle->GetTransform()->SetScale(circleScale);
		}
		else {
			m_drainEffectElapsedTime = 0.f;
			m_innerCircleElapsedTime = 0.f;
			skillFlag = false;
			phaseTwo = false;

			//이펙트 후. 데미지 줌. 
			auto gameObjects = m_innerCircle->GetCollisionObjects();

			for (auto object : gameObjects) {
				if (object->GetType() == OBJECTTYPE::MONSTER) {
					static_pointer_cast<Monster>(object)->Damaged(m_playerObject, m_playerObject->GetStatus().hitAttack * 2.5f);
					SOUND->PlaySound(L"Bianca/Bianca_Skill04_Hit02.wav", 1, 0.5f);
				}
			}
			SOUND->PlaySound(L"Bianca/Bianca_Skill04_End.wav", 1, 0.5f);
			m_circleFollowBianca = true;
			m_innerCircle->SetActive(false);
			m_outerCircle->SetActive(false);
			//SOUND->PlaySound(L"Bianca/Bianca_Skill04_End.wav", 1, 0.5f);
			cout << "비앙카 궁극기 끝\n";
		}
	}
}
