#pragma once


class GameObject;
class Transform;
class Rigidbody;

enum class ComponentType : uint8 {
	Transform,
	MeshRenderer,
	ModelRenderer,
	Camera,
	Animator,
	Light,
	Collider,
	Terrain,
	Button,
	Billboard,
	SnowBillboard,
	ParticleSystem,
	Rigidbody,
	Text,
	D2DText,
	UIPanel,
	TextButton,
	Image,
	ScrollView,
	SLIDER,
	NavMesh,
	NavMeshAgent,
	AnimationStateMachine,
	PlayerStateMachine,
	MonsterStateMachine,
	
	// ...
	Script,

	Custom,
	End
};

enum {
	FIXED_COMPONENT_COUNT = static_cast<uint8>(ComponentType::End) - 1
};

class Component
{
public:
	Component(ComponentType _type);
	virtual ~Component();

	virtual void Init() {}
	virtual void Start() {}

	virtual void Update() {}
	virtual void LateUpdate() {}
	virtual void FixedUpdate() {}

	virtual void OnDestroy() {} // 가상 소멸 전 정리 메서드
	virtual void ClearGameObjectRef() {
		// GameObject 참조 해제
		m_gameObject.reset(); // weak_ptr인 경우
		// 또는 m_gameObject = nullptr; // shared_ptr인 경우
	}

public:
	ComponentType GetType() { return m_type; }


	//GameObject 가져오기. 
	shared_ptr<GameObject> GetGameObject();
	shared_ptr<Transform> GetTransform();
	shared_ptr<Rigidbody> GetRigidbody();

private:
	friend class GameObject;
	void SetGameObject(shared_ptr<GameObject> _gameObject) { m_gameObject = _gameObject; }

protected:
	ComponentType m_type;
	//자신을 소유한 오브젝트. 
	weak_ptr<GameObject> m_gameObject;
};

