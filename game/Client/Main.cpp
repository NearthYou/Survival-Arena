#include "pch.h"
#include "Main.h"
#include "GameRuntime.h"
#include "GameRunOptions.h"
#include <shellapi.h>
#if defined(DXA_GAME_PROBE)
#include "ReferenceCrashTrace.hpp"
#endif
#include "Engine/Game.h"
#include "SceneDemo.h"
#include "RawBufferDemo.h"
#include "GroupBufferDemo.h"
#include "TextureBufferDemo.h"
#include "StructuredDemo.h"
#include "ViewportDemo.h"
#include "OrthoGraphicDemo.h"
#include "ButtonDemo.h"
#include "BillboardDemo.h"
#include "SnowBillboardDemo.h"
#include "ParticleDemo.h"
#include "UITestDemo.h"
#include "dxgidebug.h"
#include "LumiaIsland.h"
#include "StartScene.h"
#include "CharacterSelectScene.h"
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
#include "ReferenceDiagnosticsProbe.hpp"
#endif

#ifdef _DEBUG
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

void D3DMemoryLickCheck()
{
	HMODULE dxgidebugdll = GetModuleHandleW(L"dxgidebug.dll");
	decltype(&DXGIGetDebugInterface) GetDebugInterface = reinterpret_cast<decltype(&DXGIGetDebugInterface)>(GetProcAddress(dxgidebugdll, "DXGIGetDebugInterface"));

	IDXGIDebug* debug;

	GetDebugInterface(IID_PPV_ARGS(&debug));


	OutputDebugStringW(L"▼▼▼▼▼▼▼▼▼▼▼▼▼Direct3D Object ref count 메모리 누수 체크 ▼▼▼▼▼▼▼▼▼▼▼▼\r\n");

	debug->ReportLiveObjects(DXGI_DEBUG_D3D11, DXGI_DEBUG_RLO_DETAIL);

	OutputDebugStringW(L"▲▲▲▲▲▲▲▲▲▲▲▲▲반환되지 않은 IUnknown 객체가 있을경우 위에 나타남 ▲▲▲▲▲▲▲▲▲▲▲▲\r\n");

	debug->Release();
}




#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#if defined(DXA_GAME_PROBE)
    SetUnhandledExceptionFilter(ReferenceCrashTrace);
#endif
    try
    {
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    std::vector<std::wstring> arguments;
    for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
    LocalFree(argv);
    const auto options = GameRunOptions::Parse(arguments);
	const bool validateOnly = options.validateOnly;
	if (!dxa_game_runtime::Prepare(validateOnly)) return 2;
	if (validateOnly) return 0;
	srand(time(NULL));
	
	ArenaAppConfig desc;
	desc.appName = L"Survival Arena";
	desc.hInstance = hInstance;
	desc.vsync = true;
	desc.hWnd = NULL;
    desc.width = static_cast<float>(options.width);
    desc.height = static_cast<float>(options.height);
    desc.measurementFile = options.measurementFile;
    desc.measurementSeconds = options.measurementSeconds;
    desc.measurementReady = [] { return std::dynamic_pointer_cast<LumiaIsland>(SCENE->GetCurScene()) != nullptr; };
	desc.clearColor = Color(0.f, 0.f, 0.f, 0.f);
	//desc.app = make_shared<StartScene>();
	  // 초기 씬 생성 콜백 설정
	desc.createInitialScene = []() -> shared_ptr<Scene> {
		return make_shared<StartScene>();
		};

	const int result = static_cast<int>(GAME->Run(desc));


#ifdef _DEBUG
	D3DMemoryLickCheck();
#endif

	return result;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Game run failed: " << error.what() << std::endl;
        return 2;
    }
}