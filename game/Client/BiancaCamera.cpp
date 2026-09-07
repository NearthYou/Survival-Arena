#include "pch.h"
#include "BiancaCamera.h"

BiancaCamera::BiancaCamera()
{
}

void BiancaCamera::Update()
{
    auto target = m_target.lock();
    if (target)
    {
        Vec3 targetPos = target->GetTransform()->GetPosition();
        GetTransform()->SetPosition(targetPos + m_offset);
    }
}
