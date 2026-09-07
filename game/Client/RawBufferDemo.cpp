#include "pch.h"
#include "RawBufferDemo.h"
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

void RawBufferDemo::Init()
{
	RESOURCES->Init();
	m_shader = make_shared<Shader>(L"24. RawBufferDemo.fx");

	uint32 count = 10 * 8 * 3;

	shared_ptr<RawBuffer> rawBuffer = make_shared<RawBuffer>(nullptr, 0, sizeof(Output) * count);
	m_shader->GetUAV("Output")->SetUnorderedAccessView(rawBuffer->GetUAV().Get());


	//x, y, z, 쓰레드 그룹, 
	m_shader->Dispatch(0, 0, 1, 1, 1);

	vector<Output> outputs(count);
	rawBuffer->CopyFromOutput(outputs.data());

	FILE* file;
	::fopen_s(&file, "../RawBuffer.csv", "w");

	::fprintf
	(
		file,
		"GroupID(X),GroupID(Y),GroupID(Z),GroupThreadID(X),GroupThreadID(Y),GroupThreadID(Z),DispatchThreadID(X),DispatchThreadID(Y),DispatchThreadID(Z),GroupIndex\n"
	);

	for (uint32 i = 0; i < count; i++)
	{
		const Output& temp = outputs[i];

		::fprintf
		(
			file,
			"%d,%d,%d,	%d,%d,%d,	%d,%d,%d,	%d\n",
			temp.groupID[0], temp.groupID[1], temp.groupID[2],
			temp.groupThreadID[0], temp.groupThreadID[1], temp.groupThreadID[2],
			temp.dispatchThreadID[0], temp.dispatchThreadID[1], temp.dispatchThreadID[2],
			temp.groupIndex
		);
	}

	::fclose(file);

}

void RawBufferDemo::Update()
{
}

void RawBufferDemo::Render()
{
}
