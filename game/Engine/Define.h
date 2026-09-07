#pragma once
#include <stdexcept>

#define DECLARE_SINGLE(classname)			\
private:									\
	classname() { }							\
public:										\
	static classname* GetInstance()			\
	{										\
		static classname s_instance;		\
		return &s_instance;					\
	}

#define GET_SINGLE(classname)	classname::GetInstance()

#define	MSG_BOX(_message)	MessageBox(NULL, TEXT(_message), L"System Message", MB_OK)


#define CHECK(p) do { const auto dxaResult = (p); assert(SUCCEEDED(dxaResult)); if (FAILED(dxaResult)) throw std::runtime_error("Graphics operation failed."); } while (false)
#define GAME		GET_SINGLE(ArenaApplication)
#define GRAPHICS	GET_SINGLE(Graphics)
#define DEVICE		GRAPHICS->GetDevice()
#define DC			GRAPHICS->GetDeviceContext()
#define INPUT		GET_SINGLE(InputManager)
#define TIME		GET_SINGLE(TimeManager)
#define DT			TIME->GetDeltaTime()
#define RESOURCES	GET_SINGLE(ResourceManager)
#define RENDER		GET_SINGLE(RenderManager)
#define GUI			GET_SINGLE(ImGuiManager)
#define SCENE		GET_SINGLE(SceneManager)
#define SOUND		GET_SINGLE(SoundManager)
#define D2DTEXTR	GET_SINGLE(D2DTextRenderer)
#define ITEM		GET_SINGLE(ItemManger)
#define RECIPE		GET_SINGLE(RecipeManager)
#define EVENT		GET_SINGLE(EventManager)


#define CURSCENE	SCENE->GetCurScene()
#define CUR_SCENE	SCENE->GetCurScene()

#define PSM			PlayerStateMachine
#define ASM			AnimationStateMachine


#define GET_TECH(_isShadow) _isShadow == true ? 1 : 0

#define RESOLUTION_CONSTANT 0.711f


enum LayerMask {
	LAYER_DEFAULT = 0,
	LAYER_UI = 1
};