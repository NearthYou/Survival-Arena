#include "pch.h"
#include "CharacterSelectScene.h"
#include "ArenaUiTheme.h"
#include "ArenaAppearance.h"
#include <cmath>
#if defined(DXA_GAME_PROBE)
#include "GameProbe.h"
#endif
#include "LumiaIsland.h"
#include "FogOfWar.h"
#include "CameraScript.h"

#include "UIPanel.h"

#include "Graphics.h"
#include "Viewport.h"

#include "Camera.h"

#include "Material.h"
#include "ScrollView.h"

#include "Button.h"
#include "Cursor.h"

const vector<int> skinCount = {
	5, 6, 3, 4, 5,
	4, 2, 4, 5, 2,
	5, 5, 5, 3, 1,
	5, 7, 4, 6
};

const vector<wstring> characterNames = {
		L"Bianca",
		L"Nicky",
		L"Abigail",
		L"Aiden",
		L"Chiara",
		L"Daniel",
		L"Darko",
		L"DebiMarlene",
		L"Eva",
		L"Garnet",
		L"Isol",
		L"Laura",
		L"Nadine",
		L"Nathapon",
		L"Niah",
		L"Silvia",
		L"Sua",
		L"Tia",
		L"Yuki"
};

const vector<wstring> characterKoreanNames = {
		L"비앙카",
		L"니키",
		L"아비게일",
		L"에이든",
		L"키아라",
		L"다니엘",
		L"다르코",
		L"데비마를렌",
		L"이바",
		L"가넷",
		L"아이솔",
		L"라우라",
		L"나딘",
		L"나타폰",
		L"니아",
		L"실비아",
		L"수아",
		L"띠아",
		L"유키"
};

const vector<wstring> charcaterSelectVoice = {
	L"Bianca/Bianca_selected_1_ko.wav",
	L"Bianca/Bianca_selected_1_ko.wav",
	L"Nicky/Nicky_selected_1_ko.wav"
};

void CharacterSelectScene::Start()
{	
	m_defaultshader = make_shared<Shader>(L"FOW.fx");
	m_imageShader = make_shared<Shader>(L"ImageShader.fx");
	m_selectElapsedTime = 0.f;
	m_countTimer = 0.f;

	SOUND->StopAll();
	SOUND->PlayBGM(L"SFX/Select/BSER_BGM_StrategyMap.wav", 0.5f);

	CreateMainCamera();
	CreateUICamera();
	CreateLight();
	LoadCharacterSelectSceneImages();

	CreateBackGround();
	CreateScrollableCharacterList();
	CreateScrollableSkinList();

	CreateSelectedButton();

	CreateTimeProgressBar();
	CreateCursor();

	Scene::Start();
}

void CharacterSelectScene::Update()
{
#if defined(DXA_GAME_PROBE)
#if defined(DXA_GAME_PROBE_SELECTION)
    ReferenceSelectionUiProbe(m_characterList->GetScrollView(), m_selectedCharacterSkinScrollView->GetScrollView(), m_charSelectPanel->GetButton(L"GameStartButton"), m_selectCharIdx, m_selectElapsedTime, DXA_GAME_PROBE_CHARACTER, DXA_GAME_PROBE_TIMEOUT_START != 0);
#else
    static int oracleFrames = 0;
    if (++oracleFrames == 30) { ReferenceQueueFrame(L"oracle-selection.bmp"); OnCharacterSelectButtonClicked(DXA_GAME_PROBE_CHARACTER); }
#endif
#endif

	m_selectElapsedTime += DT;

	if (m_selectElapsedTime >= 45.f) {
		m_countTimer += DT;

		if (m_countTimer >= 1.f) {
			SOUND->PlaySound(L"SFX/Select/StrategyMapCount.wav", 2, 0.5f);
			m_countTimer = 0.f;
		}
	}

	if (m_selectElapsedTime >= 55.f) {
		StartLumiaIsland();
	}
	UpdateTimeProgressBar();
	Scene::Update();
}

void CharacterSelectScene::FixedUpdate()
{
	Scene::FixedUpdate();
}

void CharacterSelectScene::LateUpdate()
{
	Scene::LateUpdate();
}

void CharacterSelectScene::Render()
{
	Scene::Render();
}

void CharacterSelectScene::CreateMainCamera()
{
	// Camera
	auto camera = make_shared<GameObject>();
	//camera->GetTransform()->SetPosition(Vec3(0.f, 15.f, 15.f));
	camera->GetTransform()->SetPosition(Vec3{ 0.f, 30.f, -5.f });
	camera->AddComponent(make_shared<Camera>());
	camera->AddComponent(make_shared<CameraScript>());

	camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, true);
	//CURSCENE->Add(camera);
	Add(camera);
}

void CharacterSelectScene::CreateUICamera()
{
	// UICamera
	auto camera = make_shared<GameObject>();
	camera->GetTransform()->SetPosition(Vec3{ 0.f, 0.f, -5.f });
	camera->AddComponent(make_shared<Camera>());
	camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
	camera->GetCamera()->SetNear(0.1f);
	camera->GetCamera()->SetFar(100.0f);
	camera->GetCamera()->SetCullingMaskAll();
	camera->GetCamera()->SetCullingMaskLayerOnOff(LAYER_UI, false);
	//CURSCENE->Add(camera);
	Add(camera);
}

void CharacterSelectScene::CreateLight()
{
	// Light
	auto light = make_shared<GameObject>();
	light->AddComponent(make_shared<Light>());

	LightDesc lightDesc;
	lightDesc.ambient = Vec4(0.4f);
	lightDesc.diffuse = Vec4(1.f);
	lightDesc.specular = Vec4(0.1f);
	lightDesc.direction = Vec3(1.f, 1.f, 1.f);
	//light->GetLight()->SetLightDesc(lightDesc);
	light->GetTransform()->SetRotation(lightDesc.direction);
	light->GetTransform()->SetPosition(Vec3(0.f, 150.f, 0.f));
	static_pointer_cast<Light>(light->GetFixedComponent(ComponentType::Light))->SetLightDesc(lightDesc);
	//CURSCENE->Add(light);
	Add(light);
}

void CharacterSelectScene::CreateCursor()
{
	auto cursorObj = make_shared<GameObject>();
	cursorObj->SetName(L"MouseCursorObject");

	m_cursor = make_shared<Cursor>();
	cursorObj->AddComponent(m_cursor);
	m_cursor->SetVisible(true);

	CURSCENE->Add(cursorObj);
}

void CharacterSelectScene::LoadCharacterSelectSceneImages()
{
	LoadCharacterListSlotImages();
	LoadCharacterImages();
	LoadCharacterSkinListSlotImages();
	LoadBackGround();
	LoadCharacterFullAndHalfImages();
}

void CharacterSelectScene::LoadBackGround()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");

	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		};

	shared_ptr<Material> backGround = make_shared<Material>();
	SetupUIMaterial(backGround);
	auto backGroundTexture = RESOURCES->Load<Texture>(L"CSSceneBackGround", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharaterSelectSceneImage.png");
	backGround->SetDiffuseMap(backGroundTexture);
	MaterialDesc& backGroundDesc = backGround->GetMaterialDesc();
	backGroundDesc.ambient = Vec4(1.f);
	backGroundDesc.diffuse = Vec4(1.f);
	backGroundDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CSSceneBackGround", backGround);

	shared_ptr<Material> backGroundDeco1 = make_shared<Material>();
	SetupUIMaterial(backGroundDeco1);
	auto backGroundDeco1Texture = RESOURCES->Load<Texture>(L"CSSceneBackGroundDeco_1", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\Img_NonCharacterCard.png");
	backGroundDeco1->SetDiffuseMap(backGroundDeco1Texture);
	MaterialDesc& backGroundDeco1Desc = backGroundDeco1->GetMaterialDesc();
	backGroundDeco1Desc.ambient = Vec4(1.f);
	backGroundDeco1Desc.diffuse = Vec4(1.f);
	backGroundDeco1Desc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CSSceneBackGroundDeco_1", backGroundDeco1);

	shared_ptr<Material> CSScene_BtnPressedMat = make_shared<Material>();
	SetupUIMaterial(CSScene_BtnPressedMat);
	auto CSScene_BtnPressedMatTexture = RESOURCES->Load<Texture>(L"CSScene_BtnPressed", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\Btn_MatchingStart_01.png");
	CSScene_BtnPressedMat->SetDiffuseMap(CSScene_BtnPressedMatTexture);
	MaterialDesc& CSScene_BtnPressedMatDesc = CSScene_BtnPressedMat->GetMaterialDesc();
	CSScene_BtnPressedMatDesc.ambient = Vec4(1.f);
	CSScene_BtnPressedMatDesc.diffuse = Vec4(1.f);
	CSScene_BtnPressedMatDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CSScene_BtnPressed", CSScene_BtnPressedMat);

	shared_ptr<Material> CSScene_BtnDisabledMat = make_shared<Material>();
	SetupUIMaterial(CSScene_BtnDisabledMat);
	auto CSScene_BtnDisabledMatTexture = RESOURCES->Load<Texture>(L"CSScene_BtnDisabled", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\Btn_MatchingStart_Disabled_01.png");
	CSScene_BtnDisabledMat->SetDiffuseMap(CSScene_BtnDisabledMatTexture);
	MaterialDesc& CSScene_BtnDisabledMatDesc = CSScene_BtnDisabledMat->GetMaterialDesc();
	CSScene_BtnDisabledMatDesc.ambient = Vec4(1.f);
	CSScene_BtnDisabledMatDesc.diffuse = Vec4(1.f);
	CSScene_BtnDisabledMatDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CSScene_BtnDisabled", CSScene_BtnDisabledMat);
}

void CharacterSelectScene::LoadCharacterListSlotImages()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");

	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		}; 
	
	vector<wstring> slotNames = {
		L"CharSlotNormal",
		L"CharRandomSlotNormal",
		L"CharRandomSlotRollOver",
		L"CharSlotRollOver"
	};
	vector<wstring> fileNames = {
		L"Img_Slot_Character_Route.png",
		L"Img_Slot_CharacterList_Random.png",
		L"Img_Slot_CharacterList_Random_Over.png",
		L"Img_Slot_CharacterList_Select.png"
	};

	wstring prefixPath = L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\";
	for (int i = 0; i < slotNames.size(); i++)
	{
		shared_ptr<Material> slot = make_shared<Material>();
		SetupUIMaterial(slot);

		wstring finalPath = prefixPath + fileNames[i];

		auto slotTexture = RESOURCES->Load<Texture>(slotNames[i], finalPath);
		
		slot->SetDiffuseMap(slotTexture);
		MaterialDesc& slotDesc = slot->GetMaterialDesc();
		slotDesc.ambient = Vec4(1.f);
		slotDesc.diffuse = Vec4(1.f);
		slotDesc.specular = Vec4(1.0f);
		RESOURCES->Add(slotNames[i], slot);
	}

	shared_ptr<Material> charRandomImage = make_shared<Material>();
	SetupUIMaterial(charRandomImage);
	auto charRandomImageTexture = RESOURCES->Load<Texture>(L"CharLobbyRandom", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharLobby_Random.png");
	charRandomImage->SetDiffuseMap(charRandomImageTexture);
	MaterialDesc& charRandomImageDesc = charRandomImage->GetMaterialDesc();
	charRandomImageDesc.ambient = Vec4(1.f);
	charRandomImageDesc.diffuse = Vec4(1.f);
	charRandomImageDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CharLobbyRandom", charRandomImage);
}

void CharacterSelectScene::LoadCharacterSkinListSlotImages()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");

	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		};


	shared_ptr<Material> charSkinListSlot = make_shared<Material>();
	SetupUIMaterial(charSkinListSlot);
	auto charSkinListSlotTexture = RESOURCES->Load<Texture>(L"CharSkinSlotNormal", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\Img_SkinSlot_Basic_01_Common.png");
	charSkinListSlot->SetDiffuseMap(charSkinListSlotTexture);
	MaterialDesc& charSkinListSlotDesc = charSkinListSlot->GetMaterialDesc();
	charSkinListSlotDesc.ambient = Vec4(1.f);
	charSkinListSlotDesc.diffuse = Vec4(1.f);
	charSkinListSlotDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CharSkinSlotNormal", charSkinListSlot);

	shared_ptr<Material> charSkinListSlotRollOver = make_shared<Material>();
	SetupUIMaterial(charSkinListSlotRollOver);
	auto charSkinListSlotRollOverTexture = RESOURCES->Load<Texture>(L"CharSkinSlotRollOver", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\Img_SkinSlot_Basic_01_RollOver.png");
	charSkinListSlotRollOver->SetDiffuseMap(charSkinListSlotRollOverTexture);
	MaterialDesc& charSkinListSlotRollOverDesc = charSkinListSlotRollOver->GetMaterialDesc();
	charSkinListSlotRollOverDesc.ambient = Vec4(1.f);
	charSkinListSlotRollOverDesc.diffuse = Vec4(1.f);
	charSkinListSlotRollOverDesc.specular = Vec4(1.0f);
	RESOURCES->Add(L"CharSkinSlotRollOver", charSkinListSlotRollOver);
}

void CharacterSelectScene::LoadCharacterImages()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");


	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		};

	
	wstring prefixTag = L"CharLobby";
	wstring prefixPath = L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharacterImages\\";

	for (int i = 0; i < characterNames.size(); i++)
	{
		shared_ptr<Material> charLobbyImage = make_shared<Material>();
		SetupUIMaterial(charLobbyImage);

		wstring tag = prefixTag + characterNames[i];
		wstring path = prefixPath + characterNames[i] + L"\\" + prefixTag + L"_" + characterNames[i] + L"_S000.png";
		auto charLobbyTexture = RESOURCES->Load<Texture>(tag, path);
		
		
		charLobbyImage->SetDiffuseMap(charLobbyTexture);
		MaterialDesc& charLobbyDesc = charLobbyImage->GetMaterialDesc();
		charLobbyDesc.ambient =		Vec4(1.f);
		charLobbyDesc.diffuse =		Vec4(1.f);
		charLobbyDesc.specular =	Vec4(1.f);
		RESOURCES->Add(tag, charLobbyImage);
	}
}

void CharacterSelectScene::LoadCharacterFullAndHalfImages()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");


	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		};

	wstring prefixTagFull = L"Full";
	wstring prefixTagHalf = L"Half";
	wstring prefixPath = L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharacterImages\\";

	wstring tag, path;

	for (int i = 0; i < characterNames.size(); i++)
	{
		for (int j = 0; j < skinCount[i]; j++)
		{
			////////////Full Image////////////////////////
			shared_ptr<Material> charFullImage = make_shared<Material>();
			SetupUIMaterial(charFullImage);

			tag = L"Char" + prefixTagFull + L"_" + characterNames[i] + L"_S00" + to_wstring(j);
			path = prefixPath + characterNames[i] + L"\\" + tag + L".png";
			auto charFullTexture = RESOURCES->Load<Texture>(tag, path);

			charFullImage->SetDiffuseMap(charFullTexture);
			MaterialDesc& charFullImageDesc = charFullImage->GetMaterialDesc();
			charFullImageDesc.ambient = Vec4(1.f);
			charFullImageDesc.diffuse = Vec4(1.f);
			charFullImageDesc.specular = Vec4(1.0f);
			RESOURCES->Add(tag, charFullImage);
			////////////Full Image////////////////////////


			////////////Half Image////////////////////////
			shared_ptr<Material> charHalfImage = make_shared<Material>();
			SetupUIMaterial(charHalfImage);

			tag = L"Char" + prefixTagHalf + L"_" + characterNames[i] + L"_S00" + to_wstring(j);
			path = prefixPath + characterNames[i] + L"\\" + tag + L".png";
			auto charHalfTexture = RESOURCES->Load<Texture>(tag, path);

			charHalfImage->SetDiffuseMap(charHalfTexture);
			MaterialDesc& charHalfImageDesc = charHalfImage->GetMaterialDesc();
			charHalfImageDesc.ambient = Vec4(1.f);
			charHalfImageDesc.diffuse = Vec4(1.f);
			charHalfImageDesc.specular = Vec4(1.0f);
			RESOURCES->Add(tag, charHalfImage);
			////////////Half Image////////////////////////
		}
	}
}

void CharacterSelectScene::OnCharacterImageButtonClicked(int charIndex)
{
	SOUND->PlaySound(L"SFX/oui_Banner_Click.wav", 2, 1.f);
	m_selectCharIdx = charIndex;
}

void CharacterSelectScene::OnCharacterSelectButtonClicked(int charindex)
{
	if (charindex > 0 && charindex < 3) {
		SOUND->PlaySound(L"SFX/oui_matchClick2.wav", 2, 0.5f);
		SOUND->PlaySound(charcaterSelectVoice[charindex], 2, 0.5f);
		m_selectCharIdx = charindex;

		if (m_selectElapsedTime < 44.5f) {
			m_selectElapsedTime = 44.5f;
		}
	}
}

void CharacterSelectScene::OnCharacterSelectButtonHover()
{
	SOUND->PlaySound(L"SFX/oui_mainMenu_hover.wav", 2, 0.5f);
}

void CharacterSelectScene::StartLumiaIsland()
{
#if defined(DXA_GAME_PROBE_SELECTION)
    reference_selection_probe::Current().gameStartElapsed = m_selectElapsedTime;
#endif
	auto LumiaIslandScene = make_shared<LumiaIsland>();
	LumiaIslandScene->SetSelectedCharacter(m_selectCharIdx == 2 ? 1 : 0);
    LumiaIslandScene->SetAppearance(m_selectedAppearance);
	cout << "선택된 캐릭 인덱스 : " << m_selectCharIdx << endl;
	SCENE->ChangeScene(LumiaIslandScene);
}

void CharacterSelectScene::CreateBackGround()
{
    const float width = GRAPHICS->GetViewport().GetWidth(), height = GRAPHICS->GetViewport().GetHeight();
    const float scale = height / 768.f;
    m_backPanel = ArenaUi::Canvas(width, height, L"ArenaSelectionCanvas");
    auto panel = m_backPanel->GetUIPanel();
    panel->AddText(Vec2(274,94)*scale, L"\uce90\ub9ad\ud130 \uc120\ud0dd", 36.f*scale,
        ArenaUi::Paper(),1,Vec4(0.f),0,L"SelectionTitle",TextAlignment::Center);
    panel->AddText(Vec2(274,154)*scale, L"\uc804\ud22c \ubc29\uc2dd\uc744 \uc120\ud0dd\ud558\uc138\uc694.",16.f*scale,
        ArenaUi::Muted(),1,Vec4(0.f),0,L"SelectionInstruction",TextAlignment::Center);
    ArenaUi::Block(panel,Vec2(520,382)*scale,Vec2(2,622)*scale,ArenaUi::Muted(),L"SelectionDivider");
    ArenaUi::Block(panel,Vec2(1006,348)*scale,Vec2(558,528)*scale,ArenaUi::Panel(),L"AppearanceBoard");
    panel->AddText(Vec2(1006,605)*scale,L"\uc0c9\uc0c1 \uc120\ud0dd",16.f*scale,
        ArenaUi::Muted(),1,Vec4(0.f),0,L"AppearanceLabel",TextAlignment::Center);
    panel->AddText(Vec2(274,545)*scale,L"\uce90\ub9ad\ud130\uc640 \uc0c9\uc0c1\uc744 \uace0\ub978 \ub4a4 \uc900\ube44 \uc644\ub8cc\ub97c \ub204\ub974\uc138\uc694.",14.f*scale,
        ArenaUi::Muted(),1,Vec4(0.f),0,L"ReadyInstruction",TextAlignment::Center);
    auto image = panel->AddImageUI(Vec2(0,0),L"MainImageUI");
    auto material = RESOURCES->Get<Material>(L"CharFull_Bianca_S000")->Clone();
    material->GetMaterialDesc().diffuse = Vec4(0.25f,0.33f,0.35f,1);
    const Vec2 original = material->GetDiffuseMap()->GetSize();
    image->AddImageLayer(0,Vec2(width*0.74f,height*0.44f),original*(height*0.64f/original.y),material,9);
}

void CharacterSelectScene::CreateScrollableCharacterList()
{
    const float scale = GRAPHICS->GetViewport().GetHeight()/768.f;
    m_characterList = make_shared<GameObject>();
    m_characterList->SetName(L"CharacterScrollView");
    auto scrollView = make_shared<ScrollView>();
    m_characterList->AddComponent(scrollView);
    scrollView->Create(Vec2(274,350)*scale,Vec2(452,390)*scale,nullptr);
    scrollView->SetContentSize(Vec2(452,390)*scale);
    scrollView->SetPixelClipping(false);
    for (int i=1; i<=2; ++i)
    {
        auto panel = scrollView->AddPanel(Vec2(i==1?-108.f:108.f,0)*scale,Vec2(194,290)*scale,nullptr);
        m_roleCards[i-1] = panel;
        panel->SetBackgroundColor(ArenaUi::Panel());
        auto button = panel->AddButton(Vec2(97,145)*scale,Vec2(182,278)*scale,ArenaUi::Solid(ArenaUi::Panel()),L"RoleButton");
        button->GetGameObject()->SetName(characterNames[i-1]);
        button->GetGameObject()->GetMeshRenderer()->SetPass(2);
        button->SetHoveredMaterial(ArenaUi::Solid(Vec4(0.23f,0.3f,0.32f,1)));
        button->OnHoverEnter += [this] { OnCharacterSelectButtonHover(); };
        button->OnClick += [this,button,i] {
            UpdateFullImage(button,0);
            UpdateSkinList(button,i);
            OnCharacterImageButtonClicked(i);
            for (int card=0;card<2;++card) m_roleCards[card]->SetBackgroundColor(card==i-1?ArenaUi::Copper():ArenaUi::Panel());
        };
        auto portrait = panel->AddImageUI(Vec2(0,0));
        portrait->AddImageLayer(0,Vec2(97,122)*scale,Vec2(156,208)*scale,
            RESOURCES->Get<Material>(L"CharLobby"+characterNames[i-1])->Clone(),1);
        panel->AddText(Vec2(97,263)*scale,i==1?L"\ub9c8\ubc95\uc0ac":L"\uaca9\ud22c\uac00",19.f*scale,
            ArenaUi::Paper(),1,Vec4(0.f),0,L"RoleCaption",TextAlignment::Center);
    }
    m_characterList->SetLayerIndex(LAYER_UI);
    AddUIObject(m_characterList,true);
    RegisterUIParent(m_characterList);
}

void CharacterSelectScene::CreateScrollableSkinList()
{
    const float scale = GRAPHICS->GetViewport().GetHeight()/768.f;
    m_selectedCharacterSkinScrollView = make_shared<GameObject>();
    m_selectedCharacterSkinScrollView->SetName(L"AppearanceScrollView");
    auto scrollView = make_shared<ScrollView>();
    m_selectedCharacterSkinScrollView->AddComponent(scrollView);
    scrollView->Create(Vec2(1006,662)*scale,Vec2(552,88)*scale,nullptr);
    scrollView->SetScrollDirection(ScrollDirection::Horizontal);
    scrollView->SetContentSize(Vec2(552,88)*scale);
    scrollView->SetPixelClipping(false);
    m_selectedCharacterSkinScrollView->GetMeshRenderer()->SetActive(false);
    m_selectedCharacterSkinScrollView->SetLayerIndex(LAYER_UI);
    AddUIObject(m_selectedCharacterSkinScrollView,true);
    RegisterUIParent(m_selectedCharacterSkinScrollView);
}

void CharacterSelectScene::CreateTimeProgressBar()
{
    const float width = GRAPHICS->GetViewport().GetWidth(), height = GRAPHICS->GetViewport().GetHeight();
    const float scale = height/768.f;
    m_progressBarSize = Vec2(width-96.f*scale,4.f*scale);
    m_timeProgressBar = make_shared<GameObject>();
    m_timeProgressPanel = make_shared<UIPanel>();
    m_timeProgressBar->AddComponent(m_timeProgressPanel);
    m_timeProgressPanel->Create(Vec2(width/2,height-24.f*scale),m_progressBarSize,ArenaUi::Panel(),nullptr);
    m_timeProgressUI = m_timeProgressPanel->AddImageUI(Vec2(0,0),L"TimeProgressUI");
    m_timeProgressUI->AddImageLayer(0,m_progressBarSize/2,m_progressBarSize,ArenaUi::Solid(ArenaUi::Copper()),2);
    m_timeProgressBar->SetLayerIndex(LAYER_UI);
    AddUIObject(m_timeProgressBar,true);
    RegisterUIParent(m_timeProgressBar);
    m_countdownLabel = m_backPanel->GetUIPanel()->AddText(Vec2(width-220.f*scale,54.f*scale),L"",15.f*scale,
        ArenaUi::Muted(),1,Vec4(0.f),0,L"SelectionCountdown",TextAlignment::Center);
}

void CharacterSelectScene::UpdateTimeProgressBar()
{
    if (!m_timeProgressUI) return;
    const float ratio = std::clamp(m_selectElapsedTime/m_selectDuration,0.f,1.f);
    const Vec2 size(m_progressBarSize.x*ratio,m_progressBarSize.y);
    m_timeProgressUI->SetLayerSize(0,size);
    m_timeProgressUI->SetLayerPosition(0,size/2);
    if (m_countdownLabel) m_countdownLabel->SetText(L"\uac8c\uc784 \uc2dc\uc791\uae4c\uc9c0 " +
        std::to_wstring(static_cast<int>(std::ceil((std::max)(0.f,m_selectDuration-m_selectElapsedTime)))) + L"\ucd08");
}

void CharacterSelectScene::CreateSelectedButton()
{
    const float scale = GRAPHICS->GetViewport().GetHeight()/768.f;
    m_charSelectBtn = make_shared<GameObject>();
    m_charSelectPanel = make_shared<UIPanel>();
    m_charSelectBtn->AddComponent(m_charSelectPanel);
    m_charSelectPanel->Create(Vec2(274,653)*scale,Vec2(370,64)*scale,Vec4(0.f),nullptr);
    auto button = ArenaUi::Action(m_charSelectPanel,Vec2(185,32)*scale,Vec2(370,64)*scale,
        L"\uc900\ube44 \uc644\ub8cc",L"GameStartButton",ArenaUi::Copper(),22.f*scale,
        [this] { OnCharacterSelectButtonClicked(m_selectCharIdx); });
    button->OnHoverEnter += [this] { OnCharacterSelectButtonHover(); };
    m_charSelectBtn->SetLayerIndex(LAYER_UI);
    AddUIObject(m_charSelectBtn,true);
    RegisterUIParent(m_charSelectBtn);
}

void CharacterSelectScene::UpdateSkinList(shared_ptr<Button> button, int charIndex)
{
    const float scale = GRAPHICS->GetViewport().GetHeight()/768.f;
    auto scrollView = m_selectedCharacterSkinScrollView->GetScrollView();
    m_selectedCharacterSkinScrollView->GetMeshRenderer()->SetActive(true);
    scrollView->RemoveAllElement();
    const int count = skinCount[charIndex-1];
    scrollView->SetContentSize(Vec2(552,88)*scale);
    for (int i=0;i<count;++i)
    {
        auto panel = scrollView->AddPanel(Vec2((i-(count-1)*0.5f)*84.f,0)*scale,Vec2(74,78)*scale,nullptr);
        auto button = panel->AddButton(Vec2(37,28)*scale,Vec2(58,42)*scale,ArenaUi::Solid(ArenaAppearance::Tint(i)),L"AppearanceOption");
        button->GetGameObject()->SetName(characterNames[charIndex-1]);
        button->GetGameObject()->GetMeshRenderer()->SetPass(2);
        button->OnClick += [this, button, i]() { UpdateFullImage(button,i); };
        button->OnHoverEnter += [this] { OnCharacterSelectButtonHover(); };
        panel->AddText(Vec2(37,66)*scale,ArenaAppearance::Name(i),12.f*scale,ArenaUi::Paper(),1,Vec4(0.f),0,L"AppearanceName",TextAlignment::Center);
    }
}

void CharacterSelectScene::UpdateFullImage(shared_ptr<Button> button, int skinIndex)
{
    m_selectedAppearance = std::clamp(skinIndex,0,5);
    const wstring tag = L"CharFull_"+button->GetGameObject()->GetName()+L"_S000";
    auto material = RESOURCES->Get<Material>(tag)->Clone();
    material->GetMaterialDesc().diffuse = ArenaAppearance::Tint(m_selectedAppearance);
    const float width = GRAPHICS->GetViewport().GetWidth(), height = GRAPHICS->GetViewport().GetHeight();
    const Vec2 original = material->GetDiffuseMap()->GetSize();
    const Vec2 size = original * (height*0.64f/original.y);
    auto image = m_backPanel->GetUIPanel()->GetImageUI(L"MainImageUI");
    image->SetMaterial(0,material);
    image->SetLayerSize(0,size);
    image->SetLayerPosition(0,Vec2(width*0.74f,height*0.44f));
}
