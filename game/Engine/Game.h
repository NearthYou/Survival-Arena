#pragma once
#include <functional>
class Scene;
#include "FrameMeasurement.h"

struct ArenaAppConfig
{
	//shared_ptr<class IExecute> app = nullptr;
	std::function<shared_ptr<Scene>()> createInitialScene = nullptr;
	wstring appName = L"Survival Arena";
	HINSTANCE hInstance = 0;
	HWND hWnd = 0;
	float width = 1366;
	float height = 768;
	std::wstring measurementFile;
    unsigned measurementSeconds = 0;
    std::function<bool()> measurementReady;
	bool vsync = true;
	bool windowed = true;
	Color clearColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
};

class ArenaApplication
{
	DECLARE_SINGLE(ArenaApplication);

private:
	~ArenaApplication();

public:
	WPARAM Run(ArenaAppConfig& desc);

	ArenaAppConfig& GetGameDesc() { return _desc; }

private:
	ATOM MyRegisterClass();
	BOOL InitInstance(int cmdShow);

	void Update();
	void ShowFPS();

	static LRESULT CALLBACK WndProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);
	
private:
	ArenaAppConfig _desc;
    std::unique_ptr<FrameMeasurement> m_measurement;
};

