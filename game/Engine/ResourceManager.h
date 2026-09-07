#pragma once

#include "ResourceBase.h"

class Shader;
class Texture;
class Mesh;
class Model;
class Material;

struct PixelColor
{
	uint8_t r, g, b, a;

	PixelColor() : r(0), g(0), b(0), a(0) {}
	PixelColor(uint8_t R, uint8_t G, uint8_t B, uint8_t A) : r(R), g(G), b(B), a(A) {}

	// 편의 함수들
	Vec4 ToVec4() const { return Vec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f); }
	uint32_t ToUInt32() const { return (a << 24) | (b << 16) | (g << 8) | r; }
	bool IsTransparent() const { return a < 10; } // 거의 투명한 픽셀 체크
};

class ResourceManager
{
	DECLARE_SINGLE(ResourceManager);

public:
	void Init();

	template<typename T>
	shared_ptr<T> Load(const wstring& _key, const wstring& _path);

	template<typename T>
	bool Add(const wstring& _key, shared_ptr<T> _object);

	template<typename T>
	shared_ptr<T> Get(const wstring& _key);

	shared_ptr<Texture> GetOrAddTexture(const wstring& _key, const wstring& _path);
	shared_ptr<Model> GetOrAddModel(const wstring& _key, const wstring& _path);

	template<typename T>
	ResourceType GetResourceType();

public:
	// 특정 좌표의 픽셀 색상 조회
	bool GetPixelColor(const wstring& textureKey, int x, int y, PixelColor& outColor);

	// 텍스처 전체 픽셀 데이터 조회 (디버깅용)
	bool GetTexturePixelData(const wstring& textureKey, vector<PixelColor>& outPixels,
		uint32_t& outWidth, uint32_t& outHeight);

	// 특정 영역의 평균 색상 계산
	bool GetAverageColor(const wstring& textureKey, int startX, int startY,
		int width, int height, PixelColor& outColor);

private:
	// 헬퍼 함수: GPU 텍스처를 CPU로 복사
	bool CopyTextureToStaging(ID3D11Texture2D* sourceTexture,
		ComPtr<ID3D11Texture2D>& stagingTexture);

private:
	void CreateDefaultMesh();
	void CreateRandomTexture();
	void CreateDefaultMaterial();

private:
	wstring m_resourcePath;

private:
	using KeyObjMap = unordered_map<wstring/*key*/, shared_ptr<ResourceBase>>;
	array<KeyObjMap, RESOURCE_TYPE_COUNT> m_resources;
};

template<typename T>
shared_ptr<T>
ResourceManager::Load(const wstring& _key, const wstring& _path)
{
	auto objectType = GetResourceType<T>();
	KeyObjMap& keyObjMap = m_resources[static_cast<uint8>(objectType)];

	auto findIt = keyObjMap.find(_key);
	if (findIt != keyObjMap.end())
		return static_pointer_cast<T>(findIt->second);

	shared_ptr<T> object = make_shared<T>();
	object->Load(_path);
	keyObjMap[_key] = object;

	return object;
}

template<typename T>
bool ResourceManager::Add(const wstring& _key, shared_ptr<T> _object)
{
	ResourceType resourceType = GetResourceType<T>();
	KeyObjMap& keyObjMap = m_resources[static_cast<uint8>(resourceType)];

	auto findIt = keyObjMap.find(_key);
	if (findIt != keyObjMap.end())
		return false;

	keyObjMap[_key] = _object;
	return true;
}

template<typename T>
shared_ptr<T> ResourceManager::Get(const wstring& _key)
{
	ResourceType resourceType = GetResourceType<T>();
	KeyObjMap& keyObjMap = m_resources[static_cast<uint8>(resourceType)];

	auto findIt = keyObjMap.find(_key);
	if (findIt != keyObjMap.end())
		return static_pointer_cast<T>(findIt->second);

	return nullptr;
}

template<typename T>
ResourceType ResourceManager::GetResourceType()
{
	if (std::is_same_v<T, Texture>)
		return ResourceType::Texture;
	if (std::is_same_v<T, Mesh>)
		return ResourceType::Mesh;
	if (std::is_same_v<T, Material>)
		return ResourceType::Material;
	if (std::is_same_v<T, Model>)
		return ResourceType::Model;

	assert(false);
	return ResourceType::None;
}

