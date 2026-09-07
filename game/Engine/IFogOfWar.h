#pragma once
#include "GameObject.h"
class IFogOfWar
{
public:
    virtual ~IFogOfWar() = default;

    // 엔진에서 호출할 순수 가상 함수들
    virtual bool ShouldRenderObject(shared_ptr<GameObject> _object) = 0;
    virtual void UpdateFOWSystem() = 0;
};

