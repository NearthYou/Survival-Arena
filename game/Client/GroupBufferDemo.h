#pragma once
#include "IExecute.h"

class GroupBufferDemo : public IExecute
{

	struct Input {
		float value;
	};
	struct Output{
		uint32 groupID[3];
		uint32 groupThreadID[3];
		uint32 dispatchThreadID[3];
		uint32 groupIndex;
		float value;
	};
public:
	void Init() override;
	void Update() override;
	void Render() override;

private:
	shared_ptr<Shader> m_shader;
};

