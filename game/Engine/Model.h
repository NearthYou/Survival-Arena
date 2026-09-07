#pragma once

struct ModelBone;
struct ModelMesh;
struct ModelAnimation;

class Model : public ResourceBase, public enable_shared_from_this<Model>
{
    using Super = ResourceBase;
public:
    Model();
    ~Model();

public:
    void ReadMaterial(const wstring& _filename);
    void ReadModel(const wstring& _filename);
    void ReadAnimation(const wstring& _tag, const wstring& _filename);  // tag 매개변수 추가
    //void ReadAnimation(wstring _filename);  // 기존 버전 주석처리

    uint32 GetMaterialCount() { return static_cast<uint32>(m_materials.size()); }
    vector<shared_ptr<Material>>& GetMaterials() { return m_materials; }
    shared_ptr<Material> GetMaterialByIndex(uint32 _index) { return m_materials[_index]; }
    shared_ptr<Material> GetMaterialByName(const wstring& _name);

    uint32 GetMeshCount() { return static_cast<uint32>(m_meshes.size()); }
    vector<shared_ptr<ModelMesh>>& GetMeshes() { return m_meshes; }
    shared_ptr<ModelMesh> GetMeshByIndex(uint32 _index) { return m_meshes[_index]; }
    shared_ptr<ModelMesh> GetMeshByName(const wstring& _name);

    uint32 GetBoneCount() { return static_cast<uint32>(m_bones.size()); }
    vector<shared_ptr<ModelBone>>& GetBones() { return m_bones; }
    shared_ptr<ModelBone> GetBoneByIndex(uint32 _index) { return (_index < 0 || _index >= m_bones.size() ? nullptr : m_bones[_index]); }
    shared_ptr<ModelBone> GetBoneByName(const wstring& _name);

    uint32 GetAnimationCount() { return m_animations.size(); }
    //vector<shared_ptr<ModelAnimation>>& GetAnimations() { return m_animations; }  // 기존 버전 주석처리
    unordered_map<wstring, shared_ptr<ModelAnimation>>& GetAnimations() { return m_animations; }  // 새로운 버전

    //shared_ptr<ModelAnimation> GetAnimationByIndex(UINT _index) { return (_index < 0 || _index >= m_animations.size()) ? nullptr : m_animations[_index]; }  // 기존 버전 주석처리
    shared_ptr<ModelAnimation> GetAnimationByName(wstring _name);
    shared_ptr<ModelAnimation> GetAnimationByTag(const wstring& _tag);  // 새로운 메서드

private:
    void BindCacheInfo();

private:
    wstring _modelPath = L"../Resources/Models/";
    wstring _texturePath = L"../Resources/Textures/";

private:
    shared_ptr<ModelBone> m_root;
    vector<shared_ptr<Material>> m_materials;
    vector<shared_ptr<ModelBone>> m_bones;
    vector<shared_ptr<ModelMesh>> m_meshes;

    //vector<shared_ptr<ModelAnimation>> m_animations;  // 기존 버전 주석처리
    unordered_map<wstring, shared_ptr<ModelAnimation>> m_animations;  // 새로운 버전
};
