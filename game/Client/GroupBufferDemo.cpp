#include "pch.h"
#include "GroupBufferDemo.h"
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

void GroupBufferDemo::Init()
{
	RESOURCES->Init();
	m_shader = make_shared<Shader>(L"25. GroupBufferDemo.fx");


	//하나의 쓰레드 그룹 내에서 운영할 쓰레드 개수
	uint32 threadCount = 10 * 8 * 3;
	uint32 groupCount = 2 * 1 * 1;
	uint32 count = threadCount * groupCount;


	vector<Input> inputs(count);
	for (int32 idx = 0; idx < count; ++idx) {
		inputs[idx].value = rand() % 10000;
	}

	shared_ptr<RawBuffer> rawBuffer = make_shared<RawBuffer>(inputs.data(), sizeof(Input) * count, sizeof(Output) * count);
	
	m_shader->GetSRV("Input")->SetResource(rawBuffer->GetSRV().Get());
	m_shader->GetUAV("Output")->SetUnorderedAccessView(rawBuffer->GetUAV().Get());


	//x, y, z, 쓰레드 그룹, 
	m_shader->Dispatch(0, 0, 2, 1, 1);

	vector<Output> outputs(count);
	rawBuffer->CopyFromOutput(outputs.data());

	FILE* file;
	::fopen_s(&file, "../RawBuffer.csv", "w");

	::fprintf
	(
		file,
		"GroupID(X),GroupID(Y),GroupID(Z),GroupThreadID(X),GroupThreadID(Y),GroupThreadID(Z),DispatchThreadID(X),DispatchThreadID(Y),DispatchThreadID(Z),GroupIndex,Value\n"
	);

	for (uint32 i = 0; i < count; i++)
	{
		const Output& temp = outputs[i];

		::fprintf
		(
			file,
			"%d,%d,%d,	%d,%d,%d,	%d,%d,%d,	%d,%f\n",
			temp.groupID[0], temp.groupID[1], temp.groupID[2],
			temp.groupThreadID[0], temp.groupThreadID[1], temp.groupThreadID[2],
			temp.dispatchThreadID[0], temp.dispatchThreadID[1], temp.dispatchThreadID[2],
			temp.groupIndex, temp.value
		);
	}

	::fclose(file);

}

void GroupBufferDemo::Update()
{
}

void GroupBufferDemo::Render()
{
}
