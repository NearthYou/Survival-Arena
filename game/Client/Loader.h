#pragma once
#include <Windows.h>
#include <process.h>
#include <string>
#include "Client_Define.h"

class ArenaApplication;
class Loader
{
private:
	Loader();
	~Loader();

public:
	HRESULT Initialize(LEVEL _nextLevel);
	HRESULT Loading();
	void SetLogoText();

public:
	bool isFinished() const {
		return m_finished;
	}

private:
	LEVEL m_nextLevelID = { LEVEL::END };
	HANDLE m_thread = {};
	bool m_finished = false;

	wstring m_windowText = {};

	CRITICAL_SECTION m_criticalSection = {};

	ArenaApplication* m_gameInstance = { nullptr };

private:
	HRESULT Loading_BACKGROUND();
	HRESULT Loading_CHRACTER();
	HRESULT Loading_INGAME();
private:
	static Loader* Create(LEVEL _nextID);
};

