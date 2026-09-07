#pragma once

#include "pch.h"
#include "Delegate.h"
#include "EventClass.h"

// 이벤트 핸들러 함수 타입
using EventHandler = std::function<void(shared_ptr<EventData>)>;


class EventManager
{
    DECLARE_SINGLE(EventManager);

public:
    // 함수 객체로 구독 (람다, std::function 등)
    void Subscribe(EventType eventType, std::function<void(shared_ptr<EventData>)> handler)
    {
        m_eventDelegates[eventType] += handler;
    }

    // 멤버 함수를 바인딩하여 구독
    template<typename T>
    void Subscribe(EventType eventType, void(T::* memberFunc)(shared_ptr<EventData>), T* instance)
    {
        auto boundFunc = Delegate::Bind(memberFunc, instance);
        m_eventDelegates[eventType] += boundFunc;
    }

    // 정적 함수를 바인딩하여 구독
    void Subscribe(EventType eventType, void(*staticFunc)(shared_ptr<EventData>))
    {
        auto boundFunc = Delegate::Bind(staticFunc);
        m_eventDelegates[eventType] += boundFunc;
    }

    // 람다 함수 구독 (편의성 제공)
    template<typename Lambda>
    void Subscribe(EventType eventType, Lambda&& lambda)
    {
        std::function<void(shared_ptr<EventData>)> func = std::forward<Lambda>(lambda);
        m_eventDelegates[eventType] += func;
    }

    // 특정 이벤트 타입의 모든 구독 해제
    void UnsubscribeAll(EventType eventType)
    {
        auto it = m_eventDelegates.find(eventType);
        if (it != m_eventDelegates.end())
        {
            it->second.Reset();
        }
    }

    // 특정 핸들러 구독 해제 (Delegate의 -= 연산자 사용)
    void Unsubscribe(EventType eventType, std::function<void(shared_ptr<EventData>)> handler)
    {
        auto it = m_eventDelegates.find(eventType);
        if (it != m_eventDelegates.end())
        {
            it->second -= handler;
        }
    }

    // 즉시 이벤트 발생
    void TriggerEvent(shared_ptr<EventData> eventData)
    {
        if (!eventData) return;

        auto it = m_eventDelegates.find(eventData->GetType());
        if (it != m_eventDelegates.end() && it->second.IsBound())
        {
            // Delegate 호출
            it->second(eventData);
        }
    }

    // 지연된 이벤트 큐에 추가
    void QueueEvent(shared_ptr<EventData> eventData)
    {
        if (eventData)
        {
            m_eventQueue.push(eventData);
        }
    }

    // 큐에 있는 이벤트들 처리
    void ProcessEvents()
    {
        while (!m_eventQueue.empty())
        {
            auto eventData = m_eventQueue.front();
            m_eventQueue.pop();
            TriggerEvent(eventData);
        }
    }

    // 특정 시간 후 이벤트 발생
    void ScheduleEvent(shared_ptr<EventData> eventData, float delay)
    {
        if (!eventData) return;

        DelayedEvent delayedEvent;
        delayedEvent.eventData = eventData;
        delayedEvent.triggerTime = TIME->GetDeltaTime() + delay;
        m_delayedEvents.push_back(delayedEvent);
    }

    void Update()
    {
        float currentTime = TIME->GetDeltaTime();

        // 지연된 이벤트 처리
        auto it = m_delayedEvents.begin();
        while (it != m_delayedEvents.end())
        {
            if (currentTime >= it->triggerTime)
            {
                TriggerEvent(it->eventData);
                it = m_delayedEvents.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // 큐 이벤트 처리
        ProcessEvents();
    }

    // 디버그 정보
    void PrintEventInfo() const
    {
        for (const auto& pair : m_eventDelegates)
        {
            cout << "EventType " << (int)pair.first
                << " has " << pair.second.GetFunctionCount()
                << " subscribers" << endl;
        }
    }

    // 특정 이벤트 타입에 구독자가 있는지 확인
    bool HasSubscribers(EventType eventType) const
    {
        auto it = m_eventDelegates.find(eventType);
        return (it != m_eventDelegates.end() && it->second.IsBound());
    }

private:
    struct DelayedEvent
    {
        shared_ptr<EventData> eventData;
        float triggerTime;
    };

    // Delegate를 사용한 이벤트 핸들러 관리
    unordered_map<EventType, Delegate::Delegate<shared_ptr<EventData>>> m_eventDelegates;
    queue<shared_ptr<EventData>> m_eventQueue;
    vector<DelayedEvent> m_delayedEvents;
};

