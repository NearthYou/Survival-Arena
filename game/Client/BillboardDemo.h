#pragma once
#include "IExecute.h"

class BillboardDemo : public IExecute
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

#include "MonoBehaviour.h"
class moveScript : public MonoBehaviour
{
	virtual void Update();
};


class ForceScript : public MonoBehaviour
{
	virtual void Start() override;
	virtual void Update() override;
};