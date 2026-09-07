#include "pch.h"
#include "NavMeshAgent.h"
#include "GameObject.h"
#include "Transform.h"
#include "ModelAnimator.h"
#include "NavMesh.h"
#include "AnimationStateMachine.h"


NavMeshAgent::NavMeshAgent() : Super(ComponentType::NavMeshAgent)
{
}

NavMeshAgent::~NavMeshAgent()
{
}

void NavMeshAgent::Start()
{
    Super::Start();

    auto gameObject = GetGameObject();
    if (gameObject)
    {
        m_animator = gameObject->GetModelAnimator();

        // 씬에서 NavMesh 찾기
        auto scene = CURSCENE;
        for (auto& obj : scene->GetObjects())
        {
            auto navMesh = obj->GetFixedComponent<NavMesh>(ComponentType::NavMesh);
            if (navMesh)
            {
                m_navMesh = navMesh;
                break;
            }
        }
    }
}

void NavMeshAgent::Update()
{
    Super::Update();

    if (m_state == NavMeshAgentState::Moving)
    {
        UpdateMovement();
        UpdateAnimation();
    }
}

void NavMeshAgent::SetDestination(const Vec3& destination)
{
    if (!m_navMesh) return;
    //cout << "SetNavMeshAgent\n";
    auto transform = GetTransform();
    if (!transform) return;

    Vec3 startPos = transform->GetPosition();
    Vec3 targetPos = m_navMesh->GetNearestPointOnNavMesh(destination);

    m_navMesh->FindPath(startPos, targetPos, m_path);
    m_destination = targetPos;
    m_currentPathIndex = 0;

    if (!m_path.empty())
    {
        m_state = NavMeshAgentState::Moving;
    }
}

void NavMeshAgent::Stop()
{
    m_state = NavMeshAgentState::Idle;
    m_path.clear();
    m_currentPathIndex = 0;
    //UpdateAnimation();
}

void NavMeshAgent::UpdateMovement()
{
    if (m_path.empty() || m_currentPathIndex >= m_path.size())
    {
        m_state = NavMeshAgentState::Arrived;
        return;
    }

    auto transform = GetTransform();
    if (!transform) return;

    Vec3 currentPos = transform->GetPosition();
    Vec3 targetPos = m_path[m_currentPathIndex];

    float distance = Vec3::Distance(currentPos, targetPos);

    // 현재 웨이포인트에 도달했으면 다음으로
    if (distance <= m_stoppingDistance)
    {
        m_currentPathIndex++;
        if (m_currentPathIndex >= m_path.size())
        {
            m_state = NavMeshAgentState::Arrived;
            return;
        }
        targetPos = m_path[m_currentPathIndex];
    }

    // 이동 처리
    Vec3 direction = targetPos - currentPos;
    direction.y = 0; // Y축 고정

    if (direction.Length() > 0.01f)
    {
        direction.Normalize();

        // 회전 계산 및 적용
        float targetYaw = atan2(direction.x, direction.z) + 3.141592f; //3.141592 더해야 방향 제대로 됨

        //cout << "TargetYaw : " << targetYaw * 57.2958f << "\n";
        Vec3 currentRotation = transform->GetLocalRotation();
        Vec3 newRotation = Vec3(currentRotation.x, targetYaw * 180.0f / 3.14159f, currentRotation.z);
        transform->SetLocalRotation(newRotation);

        // 위치 업데이트
        Vec3 newPos = currentPos + direction * m_speed * DT * 2.5f;
        if (m_navMesh)
        {
            newPos = m_navMesh->GetNearestPointOnNavMesh(newPos);
        }
        transform->SetPosition(newPos);
    }
}

void NavMeshAgent::UpdateAnimation()
{
    if (!m_animator) return;

    auto gameObject = GetGameObject();
    if (gameObject)
    {
        auto stateMachine = gameObject->GetFixedComponent<AnimationStateMachine>(ComponentType::AnimationStateMachine);
        if (stateMachine)
        {
            switch (m_state)
            {
            case NavMeshAgentState::Moving:
                if (!stateMachine->IsInState(AnimationStateType::Run))
                {
                    //stateMachine->ChangeState(AnimationStateType::Run);
                }
                break;
            case NavMeshAgentState::Idle:
            case NavMeshAgentState::Arrived:
                if (!stateMachine->IsInState(AnimationStateType::Wait))
                {
                   // stateMachine->ChangeState(AnimationStateType::Wait);
                }
                break;
            }
        }
    }
}
