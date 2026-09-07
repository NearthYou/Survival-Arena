#pragma once
#include "IExecute.h"

class ViewportDemo : public IExecute
{

public:
	void Init() override;
	void Update() override;
	void Render() override;

private:
	shared_ptr<Shader> m_shader;

};

