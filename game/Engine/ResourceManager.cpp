#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Shader.h"
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include "MathUtils.h"
#include <filesystem>



void ResourceManager::Init()
{
	CreateDefaultMesh();
	CreateRandomTexture();
	CreateDefaultMaterial();
}



void ResourceManager::CreateDefaultMesh()
{
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateQuad();
		Add(L"Quad", mesh);
	}
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateCube();
		Add(L"Cube", mesh);
	}
	{
		shared_ptr<Mesh> mesh = make_shared<Mesh>();
		mesh->CreateSphere();
		Add(L"Sphere", mesh);
	}
    {
        shared_ptr<Mesh> mesh = make_shared<Mesh>();
        mesh->CreateCone();
        Add(L"Cone", mesh);
    }
}

void ResourceManager::CreateRandomTexture()
{
	shared_ptr<Texture> texture = make_shared<Texture>();
	// 
	// Create the random data.
	//
	vector<Vec4> randomValues(1024);

	for (int32 i = 0; i < 1024; ++i)
	{
		randomValues[i].x = MathUtils::Random(-1.0f, 1.0f);
		randomValues[i].y = MathUtils::Random(-1.0f, 1.0f);
		randomValues[i].z = MathUtils::Random(-1.0f, 1.0f);
		randomValues[i].w = MathUtils::Random(-1.0f, 1.0f);
	}

	D3D11_SUBRESOURCE_DATA initData;
	initData.pSysMem = randomValues.data();
	initData.SysMemPitch = 1024 * sizeof(XMFLOAT4);
	initData.SysMemSlicePitch = 0;

	//
	// Create the texture.
	//
	D3D11_TEXTURE1D_DESC texDesc;
	texDesc.Width = 1024;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;
	texDesc.ArraySize = 1;

	ComPtr<ID3D11Texture1D> randomTex;
	CHECK(DEVICE->CreateTexture1D(&texDesc, &initData, randomTex.GetAddressOf()));

	//
	// Create the resource view.
	//
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = texDesc.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	viewDesc.Texture1D.MipLevels = texDesc.MipLevels;
	viewDesc.Texture1D.MostDetailedMip = 0;

	ComPtr<ID3D11ShaderResourceView> randomTexSRV;
	CHECK(DEVICE->CreateShaderResourceView(randomTex.Get(), &viewDesc, randomTexSRV.GetAddressOf()));
	texture->SetSRV(randomTexSRV);
	Add(L"RandomTex", texture);
}

void ResourceManager::CreateDefaultMaterial()
{
	// Material
	{
		shared_ptr<Shader> renderShader = make_shared<Shader>(L"FOW.fx");
		shared_ptr<Material> material = make_shared<Material>();
		material->SetShader(renderShader);
		auto texture = RESOURCES->Load<Texture>(L"default", L"..\\Resources\\Textures\\default.png");
		material->SetDiffuseMap(texture);
		MaterialDesc& desc = material->GetMaterialDesc();
		desc.ambient = Vec4(0.5f, 0.5f, 0.5f, 1.f);
		desc.diffuse = Vec4(0.8f, 0.2f, 0.2f, 1.f);

		RESOURCES->Add(L"default", material);
	}
}

shared_ptr<Texture> ResourceManager::GetOrAddTexture(const wstring& _key, const wstring& _path)
{
	shared_ptr<Texture> texture = Get<Texture>(_key);

	if (filesystem::exists(filesystem::path(_path)) == false)
		return nullptr;

	//텍스처를 일단 로드
	texture = Load<Texture>(_key, _path);

	//없으면 새로 만들고.
	if (texture == nullptr)
	{
		texture = make_shared<Texture>();
		texture->Load(_path);
		Add(_key, texture);
	}

	//있으면 보내주기. 
	return texture;
}

shared_ptr<Model> ResourceManager::GetOrAddModel(const wstring& _key, const wstring& _path)
{
	shared_ptr<Model> model = Get<Model>(_key);

	if (model == nullptr) {
		model = make_shared<Model>();
		model->ReadModel(_path);
		Add(_key, model);
	}

	return model;
}


bool ResourceManager::GetPixelColor(const wstring& textureKey, int x, int y, PixelColor& outColor)
{
    auto texture = Get<Texture>(textureKey);
    if (!texture) return false;

    // Texture 클래스에서 ID3D11Texture2D 포인터를 얻어야 함
    // (Texture 클래스에 GetTexture2D() 함수가 있다고 가정)
    auto d3dTexture = texture->GetTexture2D();
    if (!d3dTexture) return false;

    D3D11_TEXTURE2D_DESC desc;
    d3dTexture->GetDesc(&desc);

    // 좌표 범위 체크
    if (x < 0 || x >= static_cast<int>(desc.Width) ||
        y < 0 || y >= static_cast<int>(desc.Height))
        return false;

    // Staging 텍스처 생성
    ComPtr<ID3D11Texture2D> stagingTexture;
    if (!CopyTextureToStaging(d3dTexture.Get(), stagingTexture))
        return false;

    // CPU에서 접근 가능하도록 맵핑
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = DC->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    // 픽셀 데이터 읽기 (RGBA 형식이라고 가정)
    uint8_t* data = static_cast<uint8_t*>(mapped.pData);
    int pixelIndex = y * mapped.RowPitch + x * 4;

    outColor.r = data[pixelIndex + 0];
    outColor.g = data[pixelIndex + 1];
    outColor.b = data[pixelIndex + 2];
    outColor.a = data[pixelIndex + 3];

    DC->Unmap(stagingTexture.Get(), 0);
    return true;
}

bool ResourceManager::GetTexturePixelData(const wstring& textureKey, vector<PixelColor>& outPixels,
    uint32_t& outWidth, uint32_t& outHeight)
{
    auto texture = Get<Texture>(textureKey);
    if (!texture) return false;

    auto d3dTexture = texture->GetTexture2D();
    if (!d3dTexture) return false;

    D3D11_TEXTURE2D_DESC desc;
    d3dTexture->GetDesc(&desc);

    outWidth = desc.Width;
    outHeight = desc.Height;

    ComPtr<ID3D11Texture2D> stagingTexture;
    if (!CopyTextureToStaging(d3dTexture.Get(), stagingTexture))
        return false;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = DC->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    uint8_t* data = static_cast<uint8_t*>(mapped.pData);
    outPixels.resize(outWidth * outHeight);

    for (uint32_t y = 0; y < outHeight; ++y)
    {
        for (uint32_t x = 0; x < outWidth; ++x)
        {
            int pixelIndex = y * mapped.RowPitch + x * 4;
            int arrayIndex = y * outWidth + x;

            outPixels[arrayIndex].r = data[pixelIndex + 0];
            outPixels[arrayIndex].g = data[pixelIndex + 1];
            outPixels[arrayIndex].b = data[pixelIndex + 2];
            outPixels[arrayIndex].a = data[pixelIndex + 3];
        }
    }

    DC->Unmap(stagingTexture.Get(), 0);
    return true;
}

bool ResourceManager::GetAverageColor(const wstring& textureKey, int startX, int startY,
    int width, int height, PixelColor& outColor)
{
    auto texture = Get<Texture>(textureKey);
    if (!texture) return false;

    auto d3dTexture = texture->GetTexture2D();
    if (!d3dTexture) return false;

    D3D11_TEXTURE2D_DESC desc;
    d3dTexture->GetDesc(&desc);

    // 영역 클램프
    startX = max(0, min(startX, static_cast<int>(desc.Width) - 1));
    startY = max(0, min(startY, static_cast<int>(desc.Height) - 1));
    width = min(width, static_cast<int>(desc.Width) - startX);
    height = min(height, static_cast<int>(desc.Height) - startY);

    ComPtr<ID3D11Texture2D> stagingTexture;
    if (!CopyTextureToStaging(d3dTexture.Get(), stagingTexture))
        return false;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = DC->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return false;

    uint8_t* data = static_cast<uint8_t*>(mapped.pData);

    uint64_t totalR = 0, totalG = 0, totalB = 0, totalA = 0;
    int pixelCount = 0;

    for (int y = startY; y < startY + height; ++y)
    {
        for (int x = startX; x < startX + width; ++x)
        {
            int pixelIndex = y * mapped.RowPitch + x * 4;
            totalR += data[pixelIndex + 0];
            totalG += data[pixelIndex + 1];
            totalB += data[pixelIndex + 2];
            totalA += data[pixelIndex + 3];
            ++pixelCount;
        }
    }

    if (pixelCount > 0)
    {
        outColor.r = static_cast<uint8_t>(totalR / pixelCount);
        outColor.g = static_cast<uint8_t>(totalG / pixelCount);
        outColor.b = static_cast<uint8_t>(totalB / pixelCount);
        outColor.a = static_cast<uint8_t>(totalA / pixelCount);
    }

    DC->Unmap(stagingTexture.Get(), 0);
    return true;
}

bool ResourceManager::CopyTextureToStaging(ID3D11Texture2D* sourceTexture,
    ComPtr<ID3D11Texture2D>& stagingTexture)
{
    D3D11_TEXTURE2D_DESC desc;
    sourceTexture->GetDesc(&desc);

    // Staging 텍스처 설정
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.MiscFlags = 0;

    HRESULT hr = DEVICE->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) return false;

    // GPU에서 CPU로 복사
    DC->CopyResource(stagingTexture.Get(), sourceTexture);
    return true;
}
