#include "pch.h"
#include "GameObject.h"
#include "Transform.h"
#include "Component.h"
#include "MonoBehaviour.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "Camera.h"
#include "ModelAnimation.h"
#include "ModelAnimator.h"
#include "Light.h"
#include "BaseCollider.h"
#include "Terrain.h"
#include "Button.h"
#include "Billboard.h"
#include "SnowBillboard.h"
#include "ParticleSystem.h"
#include "Renderer.h"
#include "Rigidbody.h"
#include "Text.h"
#include "D2DText.h"
#include "UIPanel.h"
#include "ImageUI.h"
#include "AABBBoxCollider.h"
#include "SphereCollider.h"
#include "AnimationStateMachine.h"
#include "NavMesh.h"
#include "NavMeshAgent.h"
#include "ScrollView.h"
#include "SliderUI.h"
#include "PlayerStateMachine.h"
#include "MonsterStateMachine.h"

GameObject::GameObject()
{

}

GameObject::~GameObject()
{

}

void GameObject::Init()
{
	for (shared_ptr<Component>& component : m_components) {
		if(component)
			component->Init();
	}

	for (shared_ptr<MonoBehaviour>& script : m_scripts) {
		if (script)
			script->Init();
	}
}

void GameObject::Start()
{
	for (shared_ptr<Component>& component : m_components) {
		if (component)
			component->Start();
	}

	for (shared_ptr<MonoBehaviour>& script : m_scripts) {
		if (script)
			script->Start();
	}
}

void GameObject::Update()
{
	for (shared_ptr<Component>& component : m_components) {
		if(component != nullptr)
			component->Update();
	}

	for (shared_ptr<MonoBehaviour>& script : m_scripts) {
		if (script && script->IsActive())
		{
			script->Update();
		}
	}
}

void GameObject::LateUpdate()
{
	for (shared_ptr<Component>& component : m_components) {
		if (component != nullptr)
			component->LateUpdate();
	}

	for (shared_ptr<MonoBehaviour>& script : m_scripts) {
		script->LateUpdate();
	}
}

void GameObject::FixedUpdate()
{
	for (shared_ptr<Component>& component : m_components) {
		if (component != nullptr)
			component->FixedUpdate();
	}

	for (shared_ptr<MonoBehaviour>& script : m_scripts) {
		script->FixedUpdate();
	}
}

shared_ptr<Component> GameObject::GetFixedComponent(ComponentType _type)
{

	uint8 index = static_cast<uint8>(_type);
	assert(index < FIXED_COMPONENT_COUNT);
	return m_components[index];
}

shared_ptr<Transform> GameObject::GetTransform()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Transform);
	if (component == nullptr) {
		component = make_shared<Transform>();
		AddComponent(component);
	}

	return static_pointer_cast<Transform>(component);
}

shared_ptr<Camera> GameObject::GetCamera()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Camera);
	return static_pointer_cast<Camera>(component);
}

shared_ptr<MeshRenderer> GameObject::GetMeshRenderer()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::MeshRenderer);
	return static_pointer_cast<MeshRenderer>(component);
}

shared_ptr<ModelRenderer> GameObject::GetModelRenderer()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::ModelRenderer);
	return static_pointer_cast<ModelRenderer>(component);
}

shared_ptr<ModelAnimator> GameObject::GetModelAnimator()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Animator);
	return static_pointer_cast<ModelAnimator>(component);
}

shared_ptr<Renderer> GameObject::GetRenderer()
{
	shared_ptr<Component> renderer = GetFixedComponent(ComponentType::MeshRenderer);
	if (renderer == nullptr)
		renderer = GetFixedComponent(ComponentType::ModelRenderer);
	if (renderer == nullptr)
		renderer = GetFixedComponent(ComponentType::Animator);
	if (renderer == nullptr)
		renderer = GetFixedComponent(ComponentType::ParticleSystem);
	if (renderer == nullptr)
		renderer = GetFixedComponent(ComponentType::Billboard);
	if (renderer == nullptr)
		renderer = GetFixedComponent(ComponentType::SnowBillboard);

	return static_pointer_cast<Renderer>(renderer);
}

shared_ptr<Light> GameObject::GetLight()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Light);
	return static_pointer_cast<Light>(component);
}

shared_ptr<BaseCollider> GameObject::GetCollider()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Collider);
	return static_pointer_cast<BaseCollider>(component);
}

shared_ptr<Terrain> GameObject::GetTerrain()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Terrain);
	return static_pointer_cast<Terrain>(component);
}

shared_ptr<Button> GameObject::GetButton()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Button);
	return static_pointer_cast<Button>(component);
}

shared_ptr<Billboard> GameObject::GetBillboard()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Billboard);
	return static_pointer_cast<Billboard>(component);
}

shared_ptr<SnowBillboard> GameObject::GetSnowBillboard()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::SnowBillboard);
	return static_pointer_cast<SnowBillboard>(component);
}

shared_ptr<ParticleSystem> GameObject::GetParticleSystem()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::ParticleSystem);
	return static_pointer_cast<ParticleSystem>(component);
}

shared_ptr<Rigidbody> GameObject::GetRigidbody()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Rigidbody);
	return static_pointer_cast<Rigidbody>(component);
}

shared_ptr<Text> GameObject::GetText()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Text);
	return static_pointer_cast<Text>(component);
}

shared_ptr<D2DText> GameObject::GetD2DText()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::D2DText);
	return static_pointer_cast<D2DText>(component);
}

shared_ptr<UIPanel> GameObject::GetUIPanel()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::UIPanel);
	return static_pointer_cast<UIPanel>(component);
}

shared_ptr<ImageUI> GameObject::GetImageUI()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::Image);
	return static_pointer_cast<ImageUI>(component);
}

shared_ptr<AnimationStateMachine> GameObject::GetAnimationStateMachine()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::AnimationStateMachine);
	return static_pointer_cast<AnimationStateMachine>(component);
}

shared_ptr<NavMesh> GameObject::GetNavMesh()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::NavMesh);
	return static_pointer_cast<NavMesh>(component);
}

shared_ptr<NavMeshAgent> GameObject::GetNavMeshAgent()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::NavMeshAgent);
	return static_pointer_cast<NavMeshAgent>(component);
}

shared_ptr<ScrollView> GameObject::GetScrollView()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::ScrollView);
	return static_pointer_cast<ScrollView>(component);
}

shared_ptr<SliderUI> GameObject::GetSliderUI()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::SLIDER);
	return static_pointer_cast<SliderUI>(component);
}

shared_ptr<PlayerStateMachine> GameObject::GetPlayerStateMachine()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::PlayerStateMachine);
	return static_pointer_cast<PlayerStateMachine>(component);
}

shared_ptr<MonsterStateMachine> GameObject::GetMonsterStateMachine()
{
	shared_ptr<Component> component = GetFixedComponent(ComponentType::MonsterStateMachine);
	return static_pointer_cast<MonsterStateMachine>(component);
}


void GameObject::AddComponent(shared_ptr<Component> _component)
{
	//enable_shared_from_this 안쓰고, this로 그냥 넘겨주면. 
	//레퍼런스 카운트를 이중으로 관리해서 안됨. 
	_component->SetGameObject(shared_from_this());

	// MonoBehaviour인지 먼저 확인
	auto script = dynamic_pointer_cast<MonoBehaviour>(_component);
	if (script) {
		// MonoBehaviour 스크립트
		m_scripts.push_back(script);
		return;
	}

	uint8 index = static_cast<uint8>(_component->GetType());

	if (index < FIXED_COMPONENT_COUNT) {
		m_components[index] = _component;
	}
	else {
		m_scripts.push_back(dynamic_pointer_cast<MonoBehaviour>(_component));
	}
}

//충돌 관련 함수. 
void GameObject::OnCollision(shared_ptr<GameObject> _other)
{
	if (GetRigidbody() != nullptr) {
		GetRigidbody()->OnCollision(_other);
	}
#ifdef _DEBUG
	//std::wcout << this->GetName() << "가 " << _other->GetName() << "와 충돌\n";
	//std::cout << "Collision\n";
#endif
}

void GameObject::OnCollisionEnter(shared_ptr<GameObject> _other)
{
#ifdef _DEBUG
	std::cout << "CollisionEnter\n";
#endif
}

void GameObject::OnCollisionExit(shared_ptr<GameObject> _other)
{
#ifdef _DEBUG
	std::cout << "CollisionExit\n";
#endif
}

void GameObject::OnDestroy()
{
	m_isDestroyed = true;
	std::wcout << L"GameObject '" << m_Name << L"' OnDestroy 호출" << std::endl;

	// 1. 모든 컴포넌트들에게 OnDestroy 알림
	for (auto& component : m_components) {
		if (component) {
			// Component에 OnDestroy 메서드가 있다면 호출
			component->OnDestroy();
		}
	}

	//// 2. 스크립트들에게 OnDestroy 알림
	//for (auto& script : m_scripts) {
	//	if (script) {
	//		script->OnDestroy();
	//	}
	//}

	// 3. 충돌 중인 다른 객체들에게 CollisionExit 이벤트 발생
	// (실제 구현에서는 Scene의 충돌 관리자를 통해 처리)

	// 4. 참조 해제
	ClearReferences();

	
}

void GameObject::ClearReferences()
{
	try {
		// 1. 컴포넌트들의 GameObject 참조 해제
		for (auto& component : m_components) {
			if (component) {
				// Component의 GameObject 참조를 nullptr로 설정
				// 또는 weak_ptr을 사용하는 경우 reset() 호출
				component->ClearGameObjectRef();
			}
		}

		//// 2. 스크립트들의 GameObject 참조 해제
		//for (auto& script : m_scripts) {
		//	if (script) {
		//		script->ClearGameObjectRef();
		//	}
		//}

		// 3. 이름 정리
		m_Name.clear();

		// 4. FOW 관련 데이터 초기화
		m_alpha = 1.0f;
		m_alphaChanged = false;

#ifdef _DEBUG
		//std::cout << "GameObject::ClearReferences 완료" << std::endl;
#endif

	}
	catch (...) {
#ifdef _DEBUG
		//std::cout << "GameObject::ClearReferences에서 예외 발생" << std::endl;
#endif
	}
}

