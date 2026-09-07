#include "pch.h"
#include "HealthBar.h"

#include "UIPanel.h"
#include "ImageUI.h"
#include "Player.h"

HealthBar::HealthBar()
{
	m_barSize = Vec2(150.f, 15.f);
	m_manaBarSize = Vec2(150.f, 5.f);
	m_offset = Vec3(0.f, 6.f, 0.f);
}

HealthBar::~HealthBar()
{
}

void HealthBar::Start()
{
	Super::Start();
	//Create();
}

void HealthBar::Update()
{
	Super::Update();
	UpdateHealthBarPosition();
}

void HealthBar::Create(Vec3 _offset)
{
	m_offset = _offset;

	//UI Panel 생성. 
	auto panelObj = make_shared<GameObject>();
	panelObj->SetName(L"HealthBarPanel_" + GetGameObject()->GetName());

	m_healthBarPanel = make_shared<UIPanel>();
	panelObj->AddComponent(m_healthBarPanel);

	m_healthBarPanel->Create(Vec2(0,0), Vec2(120, 17.5), Vec4(0.f), nullptr);
	wstring nickname;
	if (GetGameObject() != nullptr) {
		nickname = GetGameObject()->GetName();
	}

	{
		//닉네임 출력. 
		m_healthBarPanel->AddD2DText(
			Vec2(120.f * 0.5f, 17.5 * 0.5f - 66.f),      // 패널 가운데 위치
			nickname,                        // 닉네임 텍스트. 
			14.0f,                          // 폰트 크기
			Vec4(1.f, 1.f, 1.f, 1.f),      // 하얀색 (RGBA)
			1.0f,                           // 불투명도
			Vec4(0, 0, 0, 0),               // 배경색 (투명)
			0.0f,                           // 배경 불투명도
			L"NameText",                    // 텍스트 이름
			TextAlignment::Center           // 가운데 정렬
		);

	}
	//Health와 Mana를 제외한 것. 
	auto shader = make_shared<Shader>(L"ImageShader.fx");
	//체력바 배경 검은색 Background.
	{
		auto backgroundTexture = RESOURCES->Load<Texture>(L"DarkBar", L"..\\Resources\\Textures\\UI\\status\\EmptyBar_UI.png");
		auto backgroundMaterial = make_shared<Material>();
		backgroundMaterial->SetShader(shader);
		backgroundMaterial->SetRenderQueue(RenderQueue::Transparent);
		backgroundMaterial->SetTransparent(true);
		backgroundMaterial->SetDiffuseMap(backgroundTexture);


		MaterialDesc& bgDesc = backgroundMaterial->GetMaterialDesc();
		bgDesc.diffuse = Vec4(0.f, 0.f, 0.f, 0.f);

		m_healthBarUI = m_healthBarPanel->AddImageUI(Vec2(0, 0), L"HealthBar");
		m_healthBarUI->AddImageLayer(0, Vec2(60, -40), m_barSize, backgroundMaterial, 2);
	}


	//체력바 관련. 
	auto healthShader = make_shared<Shader>(L"ImageShader.fx");
	{
		auto healthMaterial = make_shared<Material>();
		healthMaterial->SetShader(healthShader);
		healthMaterial->SetRenderQueue(RenderQueue::Transparent);
		healthMaterial->SetTransparent(true);

		auto healthBarTexture = RESOURCES->Load<Texture>(L"GreenBar", L"..\\Resources\\Textures\\UI\\StatusBar\\Gauge\\Img_Main_Gage_01.png");

		healthMaterial->SetDiffuseMap(healthBarTexture);

		MaterialDesc& healthDesc = healthMaterial->GetMaterialDesc();
		healthDesc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.f);

		m_healthBarUI->AddImageLayer(1, Vec2(60, -40), m_barSize, healthMaterial, 6);
	}

	//마나 바 관련. 
	{
		auto manaMaterial = make_shared<Material>();
		manaMaterial->SetShader(healthShader);
		manaMaterial->SetRenderQueue(RenderQueue::Transparent);
		manaMaterial->SetTransparent(true);

		auto healthBarTexture = RESOURCES->Load<Texture>(L"BlueBar", L"..\\Resources\\Textures\\UI\\status\\SPBar_UI.png");

		manaMaterial->SetDiffuseMap(healthBarTexture);

		MaterialDesc& healthDesc = manaMaterial->GetMaterialDesc();
		healthDesc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.f);

		m_healthBarUI->AddImageLayer(2, Vec2(60, -30), m_manaBarSize, manaMaterial, 7);
	}

	//여기는 레벨 관련 이미지. 
	{
		auto LevelImgTexture = RESOURCES->Load<Texture>(L"ChaLevel", L"..\\Resources\\Textures\\UI\\status\\Img_ChaLevel_Bg.png");
		auto LevelMaterial = make_shared<Material>();
		auto LevelShader = make_shared<Shader>(L"ImageShader.fx");
		LevelMaterial->SetShader(shader);
		LevelMaterial->SetRenderQueue(RenderQueue::Transparent);
		LevelMaterial->SetTransparent(true);

		LevelMaterial->SetDiffuseMap(LevelImgTexture);
		MaterialDesc& LevelDesc = LevelMaterial->GetMaterialDesc();
		LevelDesc.diffuse = Vec4(1.0f, 1.0f, 1.0f, 1.f);
		m_healthBarUI->AddImageLayer(3, Vec2(-5, -40), Vec2(44, 44), LevelMaterial, 1);

		// 가운데에 하얀 텍스트 추가
		m_healthBarPanel->AddD2DText(
			Vec2(-5.f, -40.f),      // 패널 가운데 위치
			L"1",                        // 닉네임 텍스트. 
			14.0f,                          // 폰트 크기
			Vec4(1.f, 1.f, 1.f, 1.f),      // 하얀색 (RGBA)
			1.0f,                           // 불투명도
			Vec4(0, 0, 0, 0),               // 배경색 (투명)
			0.0f,                           // 배경 불투명도
			L"LevelText",                    // 텍스트 이름
			TextAlignment::Center           // 가운데 정렬
		);
	}

	CURSCENE->AddUIObject(panelObj, true);
	CURSCENE->RegisterUIParent(panelObj);
}

void HealthBar::UpdateHealthBar(int _curHP, int _maxHP, int _curMP, int _maxMP)
{
	if (_curHP == m_lastCurHP && _maxHP == m_lastMaxHP && _curMP == m_lastCurMP && _maxMP == m_lastMaxMP)
		return;

	m_lastCurHP = _curHP;
	m_lastMaxHP = _maxHP;
	m_lastCurMP = _curMP;
	m_lastMaxMP = _maxMP;

	if (m_lastMaxHP <= 0) return;

	//cout << m_lastCurHP << " " << m_lastMaxHP << "\n";
	float healthRatio = static_cast<float>(_curHP) / static_cast<float>(_maxHP);
	healthRatio = max(0.f, min(healthRatio, 1.f));

	float manaRatio = static_cast<float>(_curMP) / static_cast<float>(_maxMP);
	manaRatio = max(0.f, min(manaRatio, 1.f));

	if (auto material = m_healthBarUI->GetLayers()[1].material) {
		if (GetGameObject()->GetType() == OBJECTTYPE::MONSTER) {
			material->GetShader()->PushHealthBarData(healthRatio, manaRatio, 1);
		}
		else {
			material->GetShader()->PushHealthBarData(healthRatio, manaRatio, 0);
		}
		//material->GetShader()->PushHealthBarData(healthRatio, manaRatio);
	}
}

void HealthBar::SetVisible(bool _visible)
{
	if (m_healthBarPanel) {
		m_healthBarPanel->SetVisible(_visible);
	}
}

void HealthBar::UpdateHealthBarPosition()
{
	healthUpdateTime += DT;

	Vec3 targetPos = GetTransform()->GetPosition();

	if (Vec3::Distance(targetPos, m_lastTargetPos) < 0.03f && healthUpdateTime < 0.016f)
		return;

	if (healthUpdateTime > 0.016f)
		healthUpdateTime = 0.f;

	m_lastTargetPos = targetPos;

	Vec3 healthBarWorldPos = targetPos + m_offset;

	auto camera = CURSCENE->GetMainCamera()->GetCamera();
	Matrix worldMatrix = Matrix::Identity;
	Matrix viewMatrix = camera->GetViewMatrix();
	Matrix projMatrix = camera->GetProjectionMatrix();

	Vec3 screenPos = GRAPHICS->GetViewport().Project(healthBarWorldPos,
		worldMatrix, viewMatrix, projMatrix);

	if (m_healthBarPanel) {
		m_healthBarPanel->SetPosition(Vec2(screenPos.x, screenPos.y));
	}
}

void HealthBar::UpdateHealthBarSize(float _healthRatio)
{
	if (!m_healthBarUI) return;

	Vec2 currentHealthBarSize = Vec2(m_barSize.x * _healthRatio, m_barSize.y);
	m_healthBarUI->SetLayerSize(1, currentHealthBarSize);

	// 왼쪽 정렬
	float offsetX = (currentHealthBarSize.x - m_barSize.x) / 2.0f;
	m_healthBarUI->SetLayerPosition(1, Vec2(offsetX, 0));
}

