#include "pch.h"
#include "TimeManager.h"

void TimeManager::Init()
{
	::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_frequency));
	::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_prevCount)); // CPU Å¬·°
}

void TimeManager::Update()
{
	uint64 currentCount;
	::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));

 
    m_deltaTime = (currentCount - m_prevCount) / static_cast<float>(m_frequency);

	m_deltaTime = min(m_deltaTime, m_maxDelatTime);

	m_prevCount = currentCount;

	m_frameCount++;
	m_frameTime += m_deltaTime;
	m_gameTime += m_deltaTime;

	if (m_frameTime > 1.f)
	{
		m_fps = static_cast<uint32>(m_frameCount / m_frameTime);

		m_frameTime = 0.f;
		m_frameCount = 0;
	}
}

// TimeManager.cpp
void TimeManager::ResetDeltaTime()
{
    ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_prevCount));
    cout << "TimeManager Reset - Next frame will use fixed delta time" << endl;
}