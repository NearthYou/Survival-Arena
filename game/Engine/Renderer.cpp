#include "pch.h"
#include "Renderer.h"
#include "Material.h"
#include "Camera.h"
#include "Light.h"

Renderer::Renderer(ComponentType _componentType) : Super(_componentType)
{
}

Renderer::~Renderer()
{
}

bool Renderer::Render(bool _isShadowTech)
{
	if (m_material == nullptr)
		return false;

	if (m_material->GetCastShadow() == false && _isShadowTech == true) {
		return false;
	}

	InnerRender(_isShadowTech);
    auto tint = m_material->GetShader()->GetVector("ActorTint");
    if (tint && tint->IsValid()) { const float neutral[] = {1,1,1,1}; tint->SetFloatVector(neutral); }
	return true;
}

void Renderer::InnerRender(bool _isShadowTech)
{
    DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (_isShadowTech) {
        // ¼¨µµ¿ì ¸Ê ·»´õ¸µ
        const auto& shader = m_material->GetShader();
        shader->SetTechnique(L"shadowTech");
        shader->PushGlobalData(Light::s_MatView, Light::s_MatProjection);
    }
    else if (GRAPHICS->IsCurrentPassGeometry()) {
        // G-Buffer ÆÐ½º (µðÆÛµå ·»´õ¸µ)
        const auto& geometryShader = m_material->GetShader();
        geometryShader->SetTechnique(L"GBufferTech");
        OutlineDesc objType;
        objType.objType = 0;
        if (GetGameObject()->GetType() == OBJECTTYPE::ITEMBOX) {
            objType.objType = 1;
        }
        geometryShader->PushOutlineData(objType);

        if (geometryShader) {
            m_material->Update(); // ÅØ½ºÃ³ ¹ÙÀÎµù
            geometryShader->PushGlobalData(Camera::s_MatView, Camera::s_MatProjection);
            TransformDesc txDesc;
            txDesc.W = GetTransform()->GetWorldMatrix();
            
            geometryShader->PushTransformData(txDesc);
        }
    }
    else {
        // Æ÷¿öµå ·»´õ¸µ ¶Ç´Â Åõ¸í °´Ã¼ ·»´õ¸µ
        const auto& shader = m_material->GetShader();
        shader->SetTechnique(L"T0");
        m_material->Update();

        auto lightObj = CURSCENE->GetLight();
        if (lightObj) {
            shader->PushLightData(lightObj->GetLight()->GetLightDesc());
        }
        shader->PushGlobalData(Camera::s_MatView, Camera::s_MatProjection);
        shader->PushShadowData(Light::s_ShadowTransform);
        shader->PushTransformData(TransformDesc{ GetTransform()->GetWorldMatrix() });
    }
}
