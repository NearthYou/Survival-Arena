#pragma once

enum class KEY_TYPE
{
	UP = VK_UP,
	DOWN = VK_DOWN,
	LEFT = VK_LEFT,
	RIGHT = VK_RIGHT,

	W = 'W',
	A = 'A',
	S = 'S',
	D = 'D',

	Q = 'Q',
	E = 'E',
	R = 'R',
	Z = 'Z',
	C = 'C',
	F = 'F',
	G = 'G',
	H = 'H',
	T = 'T',

	B = 'B',

	KEY_1 = '1',
	KEY_2 = '2',
	KEY_3 = '3',
	KEY_4 = '4',
	KEY_5 = '5',
	KEY_6 = '6',
	KEY_7 = '7',
	KEY_8 = '8',


	ESC = VK_ESCAPE,

	LBUTTON = VK_LBUTTON,
	RBUTTON = VK_RBUTTON,
	LSHIFT = VK_LSHIFT,
	LCTRL = VK_LCONTROL,
};

enum class KEY_STATE
{
	NONE,
	PRESS,
	DOWN,
	UP,
	END
};

enum
{
	KEY_TYPE_COUNT = static_cast<int32>(UINT8_MAX + 1),
	KEY_STATE_COUNT = static_cast<int32>(KEY_STATE::END),
};

class InputManager
{
	DECLARE_SINGLE(InputManager);

public:
	void Init(HWND hwnd);
	void Update();

	// 누르고 있을 때
	bool GetButton(KEY_TYPE key) { return GetState(key) == KEY_STATE::PRESS; }
	// 맨 처음 눌렀을 때
	bool GetButtonDown(KEY_TYPE key) { return GetState(key) == KEY_STATE::DOWN; }
	// 맨 처음 눌렀다 뗐을 때
	bool GetButtonUp(KEY_TYPE key) { return GetState(key) == KEY_STATE::UP; }
	
	const POINT& GetMousePos() { return m_mousePos; }
	
	// 마우스 휠 지원 추가
	int GetMouseWheelDelta() const { return m_wheelDelta; }
	bool HasWheelInput() const { return m_wheelDelta != 0; }
	void OnMouseWheel(int delta) { m_wheelDelta = delta; }
	void ResetWheelDelta() { m_wheelDelta = 0; }

private:
	inline KEY_STATE GetState(KEY_TYPE key) { return m_states[static_cast<uint8>(key)]; }

private:
	HWND m_hwnd;
	vector<KEY_STATE> m_states;
	POINT m_mousePos = {};

	int m_wheelDelta = 0; // 마우스 휠 델타 저장
};

