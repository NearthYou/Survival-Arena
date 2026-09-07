#include "pch.h"
#include "Material.h"

Material::Material() : Super(ResourceType::Material)
{
	SetTransparent(false);
	SetRenderingMode(RenderingMode::Deferred);
}

Material::~Material()
{
	
}

void Material::SetShader(shared_ptr<Shader> _shader)
{
	m_shader = _shader;

	m_diffuseEffectBuffer = m_shader->GetSRV("DiffuseMap");
	m_normalEffectBuffer = m_shader->GetSRV("NormalMap");
	m_specularEffectBuffer = m_shader->GetSRV("SpecularMap");
	m_randomEffectBuffer = m_shader->GetSRV("RandomMap");
	m_cubeMapEffectBuffer = m_shader->GetSRV("CubeMap");
	m_shadowMapEffectBuffer = m_shader->GetSRV("ShadowMap");
	
}


void Material::Update()
{
	if (m_shader == nullptr)
		return;

	//RENDER->PushMaterialData(m_desc);
	m_shader->PushMaterialData(m_desc);
	if (m_diffuseMap)
		m_diffuseEffectBuffer->SetResource(m_diffuseMap->GetComPtr().Get());

	if (m_normalMap)
		m_normalEffectBuffer->SetResource(m_normalMap->GetComPtr().Get());

	if (m_specularMap)
		m_specularEffectBuffer->SetResource(m_specularMap->GetComPtr().Get());

	if (m_randomMap)
		m_randomEffectBuffer->SetResource(m_randomMap->GetComPtr().Get());

	if (m_cubeMap)
		m_cubeMapEffectBuffer->SetResource(m_cubeMap->GetComPtr().Get());

	if (m_castShadow)
		m_shadowMapEffectBuffer->SetResource(GRAPHICS->GetShadowMap()->GetComPtr().Get());

	if (m_isDecalMaterial) {
		m_shader->PushDecalData(m_decalData);

		if (m_decalTexture)
			m_shader->SetDecalTexture(m_decalTexture);

		auto depthTexture = GRAPHICS->GetDepthTexture();
		if (depthTexture)
			m_shader->SetDepthTexture(depthTexture);
	}

}

shared_ptr<Material> Material::Clone()
{
	shared_ptr<Material> material = make_shared<Material>();

	material->m_desc = m_desc;
	material->m_shader = m_shader;
	material->m_renderQueue = m_renderQueue;
	material->m_diffuseMap = m_diffuseMap;
	material->m_normalMap = m_normalMap;
	material->m_specularMap = m_specularMap;
	material->m_randomMap = m_randomMap;
	material->m_cubeMap = m_cubeMap;

	material->m_diffuseEffectBuffer = m_diffuseEffectBuffer;
	material->m_normalEffectBuffer = m_normalEffectBuffer;
	material->m_specularEffectBuffer = m_specularEffectBuffer;

	material->m_randomEffectBuffer = m_randomEffectBuffer;
	material->m_cubeMapEffectBuffer = m_cubeMapEffectBuffer;
	material->m_shadowMapEffectBuffer = m_shadowMapEffectBuffer;

	return material;
}

