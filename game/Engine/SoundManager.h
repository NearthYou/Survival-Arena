#pragma once

class SoundManager
{
	DECLARE_SINGLE(SoundManager);

	~SoundManager();

public:
	HRESULT Init();

public:
	void PlaySound(const wstring& _keyname, const int _eID, const float _volume);
	void PlayBGM(const wstring& _keyname, const float _volume);

	void StopSound(const int _eID);
	void StopAll();

	void SetChannelVolume(const int _eID, float _volume);

	int VolumeUp(const int _eID, float _volume);
	int VolumeDown(const int _eID, float _volume);

	void SetBGMVolume(float _volume);
	void SetSFXVolume(float _volume);

	int Pause(const int _eID);


private:
	void LoadSoundFile();

private:
	float m_volume;
	float m_BGMvolume = 1.f;
	float m_SFXvolume = 1.f;
	bool m_bool;
	bool m_pause;
private:
	unordered_map <wstring, FMOD::Sound*> m_mapSound;

	enum {MAXCHANNEL = 32};
	FMOD::Channel* m_Channels[MAXCHANNEL];

	FMOD::System* m_System;
    FMOD::ChannelGroup* m_BGMGroup = nullptr;
    FMOD::ChannelGroup* m_SFXGroup = nullptr;

public:
	static DWORD __stdcall BackgroundLoadingThread(LPVOID _param);
	virtual void Free();


private:
	CRITICAL_SECTION m_loadingCS;
	HANDLE m_loadingThread;
	atomic<bool> m_loadingComplete{ false };
};

