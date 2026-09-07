#pragma once

class UIResourceManager
{
	DECLARE_SINGLE(UIResourceManager);
public:
	~UIResourceManager();

    // 리소스 로딩 함수들
    shared_ptr<Material> LoadUIMaterial(const wstring& name, const wstring& texturePath);
    shared_ptr<Material> LoadUIMaterialWithColor(const wstring& name, const wstring& texturePath, const Vec4& color);
    shared_ptr<Texture> LoadTexture(const wstring& name, const wstring& path);
    shared_ptr<Shader> GetUIShader();

    void LoadAllUIResources();

    // 미리 정의된 리소스 로드
    void LoadPlayerStatusResource();
    void LoadBtnBgUIResources();
    void LoadSkillIcons();
    void LoadSkillLevelImages();
    void LoadStatusBarResources();
    void LoadEquipmentResources();
    void LoadCraftResources();
    void LoadTimeResources();
    void LoadItemBoxResources();

    // 리소스 가져오기
    shared_ptr<Material> GetMaterial(const wstring& name);
    shared_ptr<Texture> GetTexture(const wstring& name);

    // 유틸리티 함수들
    static Vec4 ColorNormalize(const Vec4& input) { return input / 255.0f; }

    // 리소스 정리
    void Cleanup();

private:
    // 리소스 캐시
    unordered_map<wstring, shared_ptr<Material>> m_materials;
    unordered_map<wstring, shared_ptr<Texture>> m_textures;
    unordered_map<wstring, shared_ptr<Shader>> m_shaders;

    // 공통 설정 함수들
    void SetupUIMaterial(shared_ptr<Material> material);
};

