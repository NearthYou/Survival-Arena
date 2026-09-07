#include "pch.h"
#include "StructuredDemo.h"
#include "GeometryHelper.h"
#include "Camera.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "Transform.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Light.h"
#include "RawBuffer.h"
#include "StructuredBuffer.h"

void StructuredDemo::Init()
{
	RESOURCES->Init();
	m_shader = make_shared<Shader>(L"24. RawBufferDemo.fx");

	vector<Matrix> inputs(500, Matrix::Identity);

	auto buffer = make_shared<StructuredBuffer>(inputs.data(), sizeof(Matrix), 500, sizeof(Matrix), 500);

	m_shader->GetSRV("Input")->SetResource(buffer->GetSRV().Get());
	m_shader->GetUAV("Output")->SetUnorderedAccessView(buffer->GetUAV().Get());

	m_shader->Dispatch(0, 0, 1, 1, 1);

	vector<Matrix> outputs(500);
	buffer->CopyFromOutput(outputs.data());

}

void StructuredDemo::Update()
{
}

void StructuredDemo::Render()
{
}
