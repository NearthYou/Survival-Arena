#pragma once
#include "IExecute.h"

class ParticleDemo : public IExecute
{

public:
	void Init() override;
	void Update() override;
	void Render() override;

private:
	shared_ptr<Shader> m_shader;


	static void Test() {
		int a = 15;
	}
};
