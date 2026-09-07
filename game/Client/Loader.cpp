#include "pch.h"
#include "Loader.h"
#include "Client_Define.h"
#include "Game.h"

Loader::Loader()
	: m_gameInstance(ArenaApplication::GetInstance())
{
}

Loader::~Loader()
{
	WaitForSingleObject(m_thread, INFINITE);
	CloseHandle(m_thread);
	DeleteCriticalSection(&m_criticalSection);
}

unsigned int APIENTRY LoadingMain(void* pArg) {
	Loader* loader = static_cast<Loader*>(pArg);

	if (FAILED(loader->Loading()))
		return 1;
	return 0;
}

HRESULT Loader::Initialize(LEVEL _nextLevel)
{
	m_nextLevelID = _nextLevel;

	InitializeCriticalSection(&m_criticalSection);
	m_thread = (HANDLE)_beginthreadex(nullptr, 0, LoadingMain, this, 0, nullptr);

	if (0 == m_thread)
		return E_FAIL;
	return S_OK;
}

HRESULT Loader::Loading()
{
	EnterCriticalSection(&m_criticalSection);

	HRESULT hr;

	switch (m_nextLevelID) {
	case LEVEL::BACKGROUND:
		hr = Loading_BACKGROUND();
		break;
	case LEVEL::CHARACTER:
		hr = Loading_CHRACTER();
		break;
	case LEVEL::INGAME:
		hr = Loading_INGAME();
		break;
	}

	LeaveCriticalSection(&m_criticalSection);

	if (FAILED(hr))
		return E_FAIL;
	return S_OK;
}

void Loader::SetLogoText()
{
	SetWindowText(m_gameInstance->GetGameDesc().hWnd, m_windowText.c_str());
}

HRESULT Loader::Loading_BACKGROUND()
{
	return E_NOTIMPL;
}

HRESULT Loader::Loading_CHRACTER()
{
	return E_NOTIMPL;
}

HRESULT Loader::Loading_INGAME()
{
	return E_NOTIMPL;
}

Loader* Loader::Create(LEVEL _nextID)
{
	Loader* loader = new Loader();
	
	if (FAILED(loader->Initialize(_nextID))) {
		MSG_BOX("Failed to Created : Loader");
		delete loader;
	}

	return loader;
}
