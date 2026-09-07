#include "pch.h"
#include "Model.h"
#include "Utils.h"
#include "FileUtils.h"
#include "tinyxml2.h"
#include <filesystem>
#include "Material.h"
#include "ModelMesh.h"
#include "ModelAnimation.h"

Model::Model() : Super(ResourceType::Model)
{
}

Model::~Model()
{
}

void Model::ReadMaterial(const wstring& _filename)
{
    wstring fullPath = _texturePath + _filename + L".xml";
    auto parentPath = filesystem::path(fullPath).parent_path();

    tinyxml2::XMLDocument* document = new tinyxml2::XMLDocument();
    tinyxml2::XMLError error = document->LoadFile(Utils::ToString(fullPath).c_str());
    assert(error == tinyxml2::XML_SUCCESS);

    tinyxml2::XMLElement* root = document->FirstChildElement();
    tinyxml2::XMLElement* materialNode = root->FirstChildElement();

    while (materialNode)
    {

        tinyxml2::XMLElement* node = nullptr;

        node = materialNode->FirstChildElement();
        wstring materialName = Utils::ToWString(node->GetText());
        shared_ptr<Material> material = RESOURCES->Get<Material>(materialName);
        if (material == nullptr) {
            material = make_shared<Material>();
            material->SetName(materialName);

            // Diffuse Texture
            node = node->NextSiblingElement();
            if (node->GetText())
            {
                wstring textureStr = Utils::ToWString(node->GetText());
                if (textureStr.length() > 0)
                {
                    auto texture = RESOURCES->GetOrAddTexture(textureStr, (parentPath / textureStr).wstring());
                    material->SetDiffuseMap(texture);
                }
            }

            // Specular Texture
            node = node->NextSiblingElement();
            if (node->GetText())
            {
                wstring texture = Utils::ToWString(node->GetText());
                if (texture.length() > 0)
                {
                    wstring textureStr = Utils::ToWString(node->GetText());
                    if (textureStr.length() > 0)
                    {
                        auto texture = RESOURCES->GetOrAddTexture(textureStr, (parentPath / textureStr).wstring());
                        material->SetSpecularMap(texture);
                    }
                }
            }

            // Normal Texture
            node = node->NextSiblingElement();
            if (node->GetText())
            {
                wstring textureStr = Utils::ToWString(node->GetText());
                if (textureStr.length() > 0)
                {
                    auto texture = RESOURCES->GetOrAddTexture(textureStr, (parentPath / textureStr).wstring());
                    material->SetNormalMap(texture);
                }
            }

            // Ambient
            {
                node = node->NextSiblingElement();

                Color color;
                color.x = node->FloatAttribute("R");
                color.y = node->FloatAttribute("G");
                color.z = node->FloatAttribute("B");
                color.w = node->FloatAttribute("A");
                material->GetMaterialDesc().ambient = color;
            }

            // Diffuse
            {
                node = node->NextSiblingElement();

                Color color;
                color.x = node->FloatAttribute("R");
                color.y = node->FloatAttribute("G");
                color.z = node->FloatAttribute("B");
                color.w = node->FloatAttribute("A");
                material->GetMaterialDesc().diffuse = color;
            }

            // Specular
            {
                node = node->NextSiblingElement();

                Color color;
                color.x = node->FloatAttribute("R");
                color.y = node->FloatAttribute("G");
                color.z = node->FloatAttribute("B");
                color.w = node->FloatAttribute("A");
                material->GetMaterialDesc().specular = color;
            }

            // Emissive
            {
                node = node->NextSiblingElement();

                Color color;
                color.x = node->FloatAttribute("R");
                color.y = node->FloatAttribute("G");
                color.z = node->FloatAttribute("B");
                color.w = node->FloatAttribute("A");
                material->GetMaterialDesc().emissive = color;
            }

            RESOURCES->Add(materialName, material);
        }

        m_materials.push_back(material);

        // Next Material
        materialNode = materialNode->NextSiblingElement();
    }

    delete document;
    BindCacheInfo();
}

void Model::ReadModel(const wstring& _filename)
{
    wstring fullPath = _modelPath + _filename + L".mesh";

    shared_ptr<FileUtils> file = make_shared<FileUtils>();
    file->Open(fullPath, FileMode::Read);

    // Bones
    {
        const uint32 count = file->Read<uint32>();

        for (uint32 i = 0; i < count; i++)
        {
            shared_ptr<ModelBone> bone = make_shared<ModelBone>();
            bone->m_index = file->Read<int32>();
            bone->m_name = Utils::ToWString(file->Read<string>());   
            bone->m_parentIndex = file->Read<int32>();
            bone->m_transform = file->Read<Matrix>();
            //수정사항. 
            bone->m_offsetMatrix = file->Read<Matrix>();

            m_bones.push_back(bone);
        }
    }

    // Mesh
    {
        const uint32 count = file->Read<uint32>();

        for (uint32 i = 0; i < count; i++)
        {
            shared_ptr<ModelMesh> mesh = make_shared<ModelMesh>();

            mesh->m_name = Utils::ToWString(file->Read<string>());
            mesh->m_boneIndex = file->Read<int32>();

            // Material
            mesh->m_materialName = Utils::ToWString(file->Read<string>());

            //VertexData
            {
                const uint32 count = file->Read<uint32>();
                vector<ModelVertexType> vertices;
                vertices.resize(count);

                void* data = vertices.data();
                file->Read(&data, sizeof(ModelVertexType) * count);
                mesh->m_geometry->AddVertices(vertices);
            }

            //IndexData
            {
                const uint32 count = file->Read<uint32>();

                vector<uint32> indices;
                indices.resize(count);

                void* data = indices.data();
                file->Read(&data, sizeof(uint32) * count);
                mesh->m_geometry->AddIndices(indices);
            }

            mesh->CreateBuffers();

            m_meshes.push_back(mesh);
        }
    }

    BindCacheInfo();
}

void Model::ReadAnimation(const wstring& _tag, const wstring& _filename)
{
    wstring fullPath = _modelPath + _filename + L".clip";

    shared_ptr<FileUtils> file = make_shared<FileUtils>();
    file->Open(fullPath, FileMode::Read);
    shared_ptr<ModelAnimation> animation = make_shared<ModelAnimation>();

    //툴에서 저장한 순서대로 넣어주기. 
    animation->m_name = Utils::ToWString(file->Read<string>());
    animation->m_duration = file->Read<float>();
    animation->m_frameRate = file->Read<float>();
    animation->m_frameCount = file->Read<uint32>();

    uint32 keyframesCount = file->Read<uint32>();

    //키 프레임 하나하나 넣어주기. 
    for (uint32 i = 0; i < keyframesCount; i++)
    {
        shared_ptr<ModelKeyframe> keyframe = make_shared<ModelKeyframe>();
        keyframe->m_boneName = Utils::ToWString(file->Read<string>());

        uint32 size = file->Read<uint32>();

        if (size > 0)
        {
            keyframe->m_transforms.resize(size);
            void* ptr = &keyframe->m_transforms[0];
            file->Read(&ptr, sizeof(ModelKeyframeData) * size);
        }

        animation->keyframes[keyframe->m_boneName] = keyframe;
    }

    //m_animations.push_back(animation);  // 기존 버전 주석처리
    m_animations[_tag] = animation;  // 새로운 버전
}

// 기존 버전 주석처리
//void Model::ReadAnimation(wstring _filename)
//{
//    wstring fullPath = _modelPath + _filename + L".clip";
//
//    shared_ptr<FileUtils> file = make_shared<FileUtils>();
//    file->Open(fullPath, FileMode::Read);
//    shared_ptr<ModelAnimation> animation = make_shared<ModelAnimation>();
//
//    //툴에서 저장한 순서대로 넣어주기. 
//    animation->m_name = Utils::ToWString(file->Read<string>());
//    animation->m_duration = file->Read<float>();
//    animation->m_frameRate = file->Read<float>();
//    animation->m_frameCount = file->Read<uint32>();
//
//    uint32 keyframesCount = file->Read<uint32>();
//
//    //키 프레임 하나하나 넣어주기. 
//    for (uint32 i = 0; i < keyframesCount; i++)
//    {
//        shared_ptr<ModelKeyframe> keyframe = make_shared<ModelKeyframe>();
//        keyframe->m_boneName = Utils::ToWString(file->Read<string>());
//
//        uint32 size = file->Read<uint32>();
//
//        if (size > 0)
//        {
//            keyframe->m_transforms.resize(size);
//            void* ptr = &keyframe->m_transforms[0];
//            file->Read(&ptr, sizeof(ModelKeyframeData) * size);
//        }
//
//        animation->keyframes[keyframe->m_boneName] = keyframe;
//    }
//
//    m_animations.push_back(animation);
//}

shared_ptr<Material> Model::GetMaterialByName(const wstring& _name)
{
    for (auto& material : m_materials)
    {
        if (material->GetName() == _name)
            return material;
    }

    return nullptr;
}

shared_ptr<ModelMesh> Model::GetMeshByName(const wstring& _name)
{
    for (auto& mesh : m_meshes)
    {
        if (mesh->m_name == _name)
            return mesh;
    }

    return nullptr;
}

shared_ptr<ModelBone> Model::GetBoneByName(const wstring& _name)
{
    for (auto& bone : m_bones)
    {
        if (bone->m_name == _name)
            return bone;
    }

    return nullptr;
}

shared_ptr<ModelAnimation> Model::GetAnimationByName(wstring _name)
{
    for (auto& animation : m_animations)
    {
        if (animation.second->m_name == _name)
            return animation.second;
    }
    return nullptr;
}

// 새로운 메서드 추가
shared_ptr<ModelAnimation> Model::GetAnimationByTag(const wstring& _tag)
{
    auto it = m_animations.find(_tag);
    if (it != m_animations.end())
        return it->second;
    return nullptr;
}

//지금까지 만든 모든 머테리얼, 모델들 관련된 부분들 하나하나 순회하면서
//캐시와 관련된 거 채워주기. 
void Model::BindCacheInfo()
{
    // Mesh에 Material 캐싱. 
    for (const auto& mesh : m_meshes)
    {
        //이미 찾았으면 스킵. 
        if (mesh->m_material != nullptr)
            continue;

        mesh->m_material = GetMaterialByName(mesh->m_materialName);
    }
    
    //Mesh에 Bone 캐싱. 
    for (const auto& mesh : m_meshes)
    {
        //이미 찾았으면 스킵. 
        if (mesh->m_bone != nullptr)
            continue;

        mesh->m_bone = GetBoneByIndex(mesh->m_boneIndex);
    }

    // Bone 계층 정보 채우기. 
    if (m_root == nullptr && m_bones.size() > 0)
    {
        m_root = m_bones[0];

        for (const auto& bone : m_bones)
        {
            if (bone->m_parentIndex >= 0)
            {
                bone->m_parent = m_bones[bone->m_parentIndex];
                bone->m_parent->children.push_back(bone);
            }
            else
            {
                bone->m_parent = nullptr;
            }
        }
    }
}
