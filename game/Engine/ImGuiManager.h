#pragma once
class ImGuiManager
{
	DECLARE_SINGLE(ImGuiManager);

public:
	void Init();
	void End();
	void Update();
	void Render();


	void ShowPickedObj();
};

