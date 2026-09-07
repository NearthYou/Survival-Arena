#include "pch.h"
#include "SoundManager.h"
#if defined(DXA_GAME_PROBE)
#include "ReferenceAudioCapture.hpp"
#endif
#include <filesystem>

namespace fs = std::filesystem;

SoundManager::~SoundManager()
{
	Free();
}

HRESULT SoundManager::Init()
{
	ZeroMemory(m_Channels, sizeof(m_Channels));
	FMOD::System_Create(&m_System);

	#if defined(DXA_GAME_PROBE_AUDIO)
    m_System->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT);
    m_System->setSoftwareFormat(48000, FMOD_SPEAKERMODE_STEREO, 0);
    m_System->setDSPBufferSize(512, 2);
#elif defined(DXA_GAME_PROBE)
    m_System->setOutput(FMOD_OUTPUTTYPE_NOSOUND);
#endif
    m_System->init(32, FMOD_INIT_NORMAL, nullptr);
	if (m_System == nullptr)
		return E_FAIL;

	FMOD::ChannelGroup* master = nullptr;
    if (m_System->createChannelGroup("BGM", &m_BGMGroup) != FMOD_OK ||
        m_System->createChannelGroup("SFX", &m_SFXGroup) != FMOD_OK ||
        m_System->getMasterChannelGroup(&master) != FMOD_OK ||
        master->addGroup(m_BGMGroup) != FMOD_OK || master->addGroup(m_SFXGroup) != FMOD_OK)
        return E_FAIL;
    m_BGMGroup->setVolume(m_BGMvolume);
    m_SFXGroup->setVolume(m_SFXvolume);
    #if defined(DXA_GAME_PROBE_AUDIO)
    ReferenceAttachAudioCapture(m_System, &m_loadingComplete);
#endif
    LoadSoundFile();

	return S_OK;
}

void SoundManager::PlaySound(const wstring& _keyname, const int _eID, const float _volume)
{
	auto iter = m_mapSound.find(_keyname);

	if (iter == m_mapSound.end()) return;

	FMOD_BOOL play = FALSE;
	m_System->playSound(iter->second, m_SFXGroup, false, &m_Channels[_eID]);
	m_Channels[_eID]->setVolume(_volume);

	m_System->update();
}

void SoundManager::PlayBGM(const wstring& _keyname, const float _volume)
{
	auto iter = m_mapSound.find(_keyname);

	if (iter == m_mapSound.end()) return;

	FMOD_BOOL play = FALSE;
	m_System->playSound(iter->second, m_BGMGroup, false, &m_Channels[0]);

	m_Channels[0]->setMode(FMOD_LOOP_NORMAL);
	m_Channels[0]->setVolume(_volume);

	m_System->update();
}

void SoundManager::StopSound(const int _eID)
{
	m_Channels[_eID]->stop();
}

void SoundManager::StopAll()
{
    if (m_BGMGroup) m_BGMGroup->stop();
    if (m_SFXGroup) m_SFXGroup->stop();
}

void SoundManager::SetChannelVolume(const int _eID, float _volume)
{
	m_Channels[_eID]->setVolume(_volume);
	m_System->update();
}

int SoundManager::VolumeUp(const int _eID, float _volume)
{
	m_volume += _volume;

	if(m_volume > 1.0f){
		m_volume = 1.0f;
	}

	m_Channels[_eID]->setVolume(m_volume);
	return 0;
}

int SoundManager::VolumeDown(const int _eID, float _volume)
{
	m_volume -= _volume;

	if (m_volume < 0.0f) {
		m_volume = 0.0f;
	}

	m_Channels[_eID]->setVolume(m_volume);
	return 0;
}

void SoundManager::SetBGMVolume(float _volume)
{
    m_BGMvolume = _volume;
    if (m_BGMGroup) m_BGMGroup->setVolume(_volume);
}


void SoundManager::SetSFXVolume(float _volume)
{
    m_SFXvolume = _volume;
    if (m_SFXGroup) m_SFXGroup->setVolume(_volume);
}

int SoundManager::Pause(const int _eID)
{
	m_pause = !m_Channels[_eID]->getPaused(&m_pause);
	m_Channels[_eID]->setPaused(m_pause);
	return 0;
}

void SoundManager::LoadSoundFile()
{
	InitializeCriticalSection(&m_loadingCS);

	m_loadingThread = CreateThread(nullptr, 0, BackgroundLoadingThread, this, 0, nullptr);
	m_System->update();
}


DWORD __stdcall SoundManager::BackgroundLoadingThread(LPVOID _param)
{
	SoundManager* soundManager = static_cast<SoundManager*>(_param);

	try {
		EnterCriticalSection(&soundManager->m_loadingCS);

		const wstring soundFolderPath = L"..\\Resources\\Sounds\\";

		// 백그라운드에서 직접 사운드 파일들 로딩
		for (const auto& iter : fs::recursive_directory_iterator(soundFolderPath)) {
			if (!iter.is_regular_file())
				continue;

			const fs::path& fullPath = iter.path();
			std::string sFullPath = fullPath.string();

			// FMOD는 thread-safe이므로 직접 생성 가능
			FMOD::Sound* sound = nullptr;
			FMOD_RESULT result = soundManager->m_System->createSound(
				sFullPath.c_str(),
				FMOD_LOOP_OFF,
				0,
				&sound
			);

			if (result == FMOD_OK) {
				fs::path relativePath = fs::relative(fullPath, soundFolderPath);
				std::wstring wkey = relativePath.generic_wstring();

				// Critical Section으로 map 접근만 보호
				soundManager->m_mapSound[wkey] = sound;
			}
		}

		// 시스템 업데이트도 백그라운드에서 가능
		soundManager->m_System->update();

		LeaveCriticalSection(&soundManager->m_loadingCS);
		soundManager->m_loadingComplete = true;

		std::cout << "Background sound loading completed successfully.\n";
	}
	catch (...) {
		LeaveCriticalSection(&soundManager->m_loadingCS);
		OutputDebugStringA("Failed background sound loading...\n");
	}

	return 0;
}

void SoundManager::Free()
{

	for (auto& pair : m_mapSound) {
		pair.second->release();
	}
	m_mapSound.clear();
	m_System->release();
	m_System->close();
}
