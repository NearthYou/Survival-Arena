#include "pch.h"
#include "UIResourceManager.h"

UIResourceManager::~UIResourceManager()
{

}

void UIResourceManager::SetupUIMaterial(shared_ptr<Material> material)
{
    if (!m_shaders[L"UIShader"])
    {
        m_shaders[L"UIShader"] = make_shared<Shader>(L"ImageShader.fx");
    }

    material->SetShader(m_shaders[L"UIShader"]);
    material->SetRenderQueue(RenderQueue::Transparent);
    material->SetTransparent(true);
    material->SetRenderingMode(RenderingMode::Forward);
}

shared_ptr<Material> UIResourceManager::LoadUIMaterial(const wstring& name, const wstring& texturePath)
{
    // 이미 로드된 경우 반환
    if (m_materials.find(name) != m_materials.end())
    {
        return m_materials[name];
    }

    auto material = make_shared<Material>();
    SetupUIMaterial(material);

    auto texture = LoadTexture(name + L"_Texture", texturePath);
    material->SetDiffuseMap(texture);

    MaterialDesc& desc = material->GetMaterialDesc();
    desc.ambient = Vec4(1.f);
    desc.diffuse = Vec4(1.f);
    desc.specular = Vec4(1.f);

    m_materials[name] = material;
    RESOURCES->Add(name, material);

    return material;
}

shared_ptr<Material> UIResourceManager::LoadUIMaterialWithColor(const wstring& name, const wstring& texturePath, const Vec4& color)
{
    if (m_materials.find(name) != m_materials.end())
    {
        return m_materials[name];
    }

    auto material = make_shared<Material>();
    SetupUIMaterial(material);

    auto texture = LoadTexture(name + L"_Texture", texturePath);
    material->SetDiffuseMap(texture);

    MaterialDesc& desc = material->GetMaterialDesc();
    desc.ambient = Vec4(1.f);
    desc.diffuse = color;
    desc.specular = Vec4(1.0f);

    m_materials[name] = material;
    RESOURCES->Add(name, material);

    return material;
}

shared_ptr<Texture> UIResourceManager::LoadTexture(const wstring& name, const wstring& path)
{
    if (m_textures.find(name) != m_textures.end())
    {
        return m_textures[name];
    }

    auto texture = RESOURCES->Load<Texture>(name, path);
    m_textures[name] = texture;

    return texture;
}

shared_ptr<Shader> UIResourceManager::GetUIShader()
{
    if (!m_shaders[L"UIShader"])
    {
        m_shaders[L"UIShader"] = make_shared<Shader>(L"ImageShader.fx");
    }
    return m_shaders[L"UIShader"];
}

void UIResourceManager::LoadAllUIResources()
{
    LoadBtnBgUIResources();
    LoadSkillIcons();
    LoadSkillLevelImages();
    LoadStatusBarResources();
    LoadEquipmentResources();
    LoadCraftResources();
    LoadTimeResources();
    LoadItemBoxResources();
    LoadPlayerStatusResource();
}

void UIResourceManager::LoadPlayerStatusResource()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\CharStatIcon\\";

    vector<wstring> statIconNames = {
        L"AttackPower", L"SkillAmpRatio", L"IncreaseBasicAttackDamageRatio",
        L"Defense", L"AttackSpeedRatio", L"CooldownReduction",
        L"CriticalStrikeChance", L"MoveSpeedRatio"
    };

    vector<Vec4> statColors = {
        UIResourceManager::ColorNormalize(Vec4(218, 187, 102, 255)),
        UIResourceManager::ColorNormalize(Vec4(211, 160, 221, 255)),
        UIResourceManager::ColorNormalize(Vec4(209, 120, 66, 255)),
        UIResourceManager::ColorNormalize(Vec4(124, 175, 203, 255)),
        UIResourceManager::ColorNormalize(Vec4(171, 162, 118, 255)),
        UIResourceManager::ColorNormalize(Vec4(200, 200, 200, 255)),
        UIResourceManager::ColorNormalize(Vec4(236, 96, 113, 255)),
        UIResourceManager::ColorNormalize(Vec4(200, 200, 200, 255))
    };

    for (int i = 0; i < statIconNames.size(); i++)
    {
        wstring materialName = L"Ico_ChaStat_" + statIconNames[i];
        wstring texturePath = basePath + materialName + L".png";
        LoadUIMaterialWithColor(materialName, texturePath, statColors[i]);
    }
}

void UIResourceManager::LoadBtnBgUIResources()
{
    wstring basePath = L"..\\Resources\\Textures\\";

    // 공통 UI 리소스들
    //LoadUIMaterial(L"ItemBoxPanel", basePath + L"ItemBox_UI\\ItemBox_BackGround.png");
    
    // 슬롯 등급별 이미지들
    vector<wstring> gradeNames = { L"Common", L"Uncommon", L"Rare", L"Epic", L"Legendary" };
    for (const auto& grade : gradeNames)
    {
        LoadUIMaterial(L"Img_Item_Slot_" + grade, basePath + L"UI_Btn\\Img_Item_Slot_" + grade + L".png");
    }
}

void UIResourceManager::LoadSkillIcons()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\SkillIcon\\";

    // 니키 스킬 아이콘들
    vector<wstring> nickySkillIcons = {
        L"SkillIcon_1033100", L"SkillIcon_1033200", L"SkillIcon_1033300",
        L"SkillIcon_1033400", L"SkillIcon_1033500"
    };

    vector<wstring> skillTags = { L"P", L"Q", L"W", L"E", L"R" };

    for (int i = 0; i < nickySkillIcons.size(); i++)
    {
        LoadUIMaterial(L"Nicky" + skillTags[i], basePath + nickySkillIcons[i] + L".png");
    }

    // 비앙카 스킬 아이콘들
    vector<wstring> biancaSkillIcons = {
        L"SkillIcon_1042100", L"SkillIcon_1042200", L"SkillIcon_1042300",
        L"SkillIcon_1042400", L"SkillIcon_1042500"
    };

    for (int i = 0; i < biancaSkillIcons.size(); i++)
    {
        LoadUIMaterial(L"Bianca" + skillTags[i], basePath + biancaSkillIcons[i] + L".png");
    }
}

void UIResourceManager::LoadSkillLevelImages()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\StatusBar\\SkillSlot\\";

    LoadUIMaterial(L"Img_Skill_LevelGageBg", basePath + L"Img_Skill_LevelGageBg.png");


    vector<wstring> skillLevelBg_Five = {
        L"UI_SkillLevelBg_Five", L"UI_SkillLevelBg_Five_LV1", L"UI_SkillLevelBg_Five_LV2",
        L"UI_SkillLevelBg_Five_LV3", L"UI_SkillLevelBg_Five_LV4", L"UI_SkillLevelBg_Five_LV5"
    };

    vector<wstring> skillLevelBg_Three = {
       L"UI_SkillLevelBg_Three", L"UI_SkillLevelBg_Three_LV1", L"UI_SkillLevelBg_Three_LV2",
       L"UI_SkillLevelBg_Three_LV3"
    };

    for (int i = 0; i < skillLevelBg_Five.size(); i++)
    {
        LoadUIMaterial(skillLevelBg_Five[i], basePath + skillLevelBg_Five[i] + L".png");
    }

    for (int i = 0; i < skillLevelBg_Three.size(); i++)
    {
        LoadUIMaterial(skillLevelBg_Three[i], basePath + skillLevelBg_Three[i] + L".png");
    }
}

void UIResourceManager::LoadStatusBarResources()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\status\\";

    LoadUIMaterial(L"HPBar_UI", basePath + L"HPBar_UI.png");
    LoadUIMaterial(L"SPBar_UI", basePath + L"SPBar_UI.png");
    LoadUIMaterial(L"Btn_LevelUp_MouseOver", basePath + L"Btn_LevelUp_MouseOver.png");
}

void UIResourceManager::LoadEquipmentResources()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\CharEquipmentIcon\\";
    vector<wstring> slotTypes = { L"Weapon", L"Armor", L"Head", L"Arm", L"Leg" };

    for (const auto& slotType : slotTypes)
    {
        LoadUIMaterial(L"Ico_Status_" + slotType, basePath + L"Ico_Status_" + slotType + L".png");
    }
}

void UIResourceManager::LoadCraftResources()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\StatusBar\\Gauge\\";
    LoadUIMaterial(L"BlueBar", basePath + L"Img_HyperloopGauge_LumiaIsland.png");
}

void UIResourceManager::LoadTimeResources()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\time\\";

    LoadUIMaterial(L"Time_UI_BG", basePath + L"Time_UI_Bg.png");
    LoadUIMaterial(L"DAY_UI_BG", basePath + L"Img_HUD_Union.png");
    LoadUIMaterial(L"SUN_UI_ICON", basePath + L"Ico_DaySun.png");
}

void UIResourceManager::LoadItemBoxResources()
{
    wstring basePath = L"..\\Resources\\Textures\\UI\\";

    LoadUIMaterial(L"ItemBoxPanel", basePath + L"ItemBox_UI\\ItemBox_BackGround.png");
}

shared_ptr<Material> UIResourceManager::GetMaterial(const wstring& name)
{
    auto it = m_materials.find(name);
    if (it != m_materials.end())
    {
        return it->second;
    }
    return nullptr;
}

shared_ptr<Texture> UIResourceManager::GetTexture(const wstring& name)
{
    auto it = m_textures.find(name);
    if (it != m_textures.end())
    {
        return it->second;
    }
    return nullptr;
}

void UIResourceManager::Cleanup()
{
    m_materials.clear();
    m_textures.clear();
    m_shaders.clear();
}
