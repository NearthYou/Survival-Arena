#pragma once

#define WIN32_LEAN_AND_MEAN


#include "Types.h"
#include "Define.h"

#include <string>
// STL
#include <memory>
#include <iostream>
#include <array>
#include <vector>
#include <queue>
#include <list>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <sstream>  // wstringstream 사용 시
#include <iomanip>  // setprecision 사용 시
using namespace std;

// WIN
#include <windows.h>
#include <assert.h>
#include <optional>

// DX
#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/DirectXTex.inl>
using namespace DirectX;
using namespace Microsoft::WRL;
#include <FX11/d3dx11effect.h>

//DX2D
#include <d2d1.h>           // Direct2D 기본 API
#include <d2d1helper.h>     // Direct2D 헬퍼 함수들
#include <dwrite.h>         // DirectWrite (텍스트 렌더링용)
#include <wincodec.h>       // Windows Imaging Component
#include <d2d1_1.h>         // Direct2D 1.1 API (Windows 8+)
#include <d2d1effects.h>    // Direct2D 이미지 효과 (Windows 8+)

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")


// Assimp
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>
#pragma pop_macro("max")
#pragma pop_macro("min")


//ImGUI
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "ImGuizmo.h"

// Libs
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")


#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/DirectXTex_debug.lib")
#pragma comment(lib, "FX11/Effects11d.lib")
#pragma comment(lib, "Assimp/assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "DirectXTex/DirectXTex.lib")
#pragma comment(lib, "FX11/Effects11.lib")
#pragma comment(lib, "Assimp/assimp-vc143-mt.lib")
#endif

#include "FMOD/fmod.hpp"
#include "FMOD/fmod_errors.h"
#pragma comment(lib, "FMOD/fmod_vc.lib")

// Managers
#include "Game.h"
#include "Texture.h"
#include "Graphics.h"
#include "InputManager.h"
#include "TimeManager.h"
#include "ResourceManager.h"
#include "SoundManager.h"
#include "ImGuiManager.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "D2DTextRenderer.h"
#include "EventManager.h"
#include "EventClass.h"


// Engine
#include "VertexData.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "Shader.h"
#include "IExecute.h"

#include "GameObject.h"
#include "Transform.h"
#include "Mesh.h"
