#include "pch.h"
#include "SkillDecalIndicator.h"

SkillDecalIndicator::SkillDecalIndicator()
{
}

SkillDecalIndicator::~SkillDecalIndicator()
{
}

void SkillDecalIndicator::Start()
{
	Super::Start();

	CreateDecalObject();
	CreateDecalTextures();
	CreateDecalMaterial();

	ShowIndicator(true);
}

void SkillDecalIndicator::Update()
{
	Super::Update();

	if (!m_isVisible) return;
	UpdateForMousePosition();

	if (m_needsUpdate) {
		switch (m_decalType) {
		case SkillDecalType::LINE:
			UpdateLineDecal();
			break;
		case SkillDecalType::CIRCLE:
			UpdateCircleDecal();
			break;
		case SkillDecalType::CONE:
			UpdateConeDecal();
			break;
		}
		m_needsUpdate = false;
	}
}

void SkillDecalIndicator::SetSkillDecal(SkillDecalType _type, float _range, float _width)
{
	m_decalType = _type;
	m_range = _range;
	m_width = _width;
	m_needsUpdate = true;

	if (m_decalMaterial) {
		shared_ptr<Texture> texture;
		switch (_type) {
		case SkillDecalType::LINE:
			texture = m_lineTexture;
			break;
		case SkillDecalType::CIRCLE:
			texture = m_circleTexture;
			break;
		case SkillDecalType::CONE:
			texture = m_coneTexture;
			break;
		}

		if (texture) {
			m_decalMaterial->SetDecalTexture(texture);
		}
	}

}

void SkillDecalIndicator::SetStartPosition(const Vec3& _pos)
{
	m_startPos = _pos;
	m_needsUpdate = true;
}

void SkillDecalIndicator::SetTargetPosition(const Vec3& _pos)
{
	m_targetPos = _pos;
	m_needsUpdate = true;
}

void SkillDecalIndicator::SetColor(const Vec4& _color)
{
	m_color = _color;
	m_needsUpdate = true;
}

void SkillDecalIndicator::ShowIndicator(bool _show)
{
	if (_show != m_isVisible) {
		m_needsUpdate = true;
	}
	m_isVisible = _show;
}

void SkillDecalIndicator::UpdateForMousePosition()
{
	Vec3 mouseWorldPos = GetMouseWorldPostion();
	SetTargetPosition(mouseWorldPos);
}

void SkillDecalIndicator::CreateDecalObject()
{
	auto meshRenderer = make_shared<MeshRenderer>();
	GetGameObject()->AddComponent(meshRenderer);

	m_decalMesh = RESOURCES->Get<Mesh>(L"Cube");

	meshRenderer->SetMesh(m_decalMesh);
}

void SkillDecalIndicator::CreateDecalMaterial()
{
	m_decalMaterial = make_shared<Material>();

	auto shader = make_shared<Shader>(L"SkillDecal.fx");
	m_decalMaterial->SetShader(shader);
	m_decalMaterial->SetAsDecalMaterial(true);
	//m_decalMaterial->SetTransparent(true);
	//m_decalMaterial->SetRenderQueue(RenderQueue::Transparent);
	if (m_lineTexture) {
		m_decalMaterial->SetDecalTexture(m_lineTexture);
	}

	if (auto meshRenderer = GetGameObject()->GetMeshRenderer()) {
		meshRenderer->SetMaterial(m_decalMaterial);
	}
}

void SkillDecalIndicator::CreateDecalTextures()
{
	// 스킬 데칼 텍스처들 로드
	
	m_lineTexture = RESOURCES->Load<Texture>(L"DecalLine", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharacterImages\\Bianca\\CharLobby_Bianca_S000.png");
	m_circleTexture = RESOURCES->Load<Texture>(L"DecalCircle", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharacterImages\\Bianca\\CharLobby_Bianca_S000.png");
	m_coneTexture = RESOURCES->Load<Texture>(L"DecalCone", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharacterImages\\Bianca\\CharLobby_Bianca_S000.png");
	m_rectangleTexture = RESOURCES->Load<Texture>(L"DecalRect", L"..\\Resources\\Textures\\UI\\CharacterSelectScene\\CharacterImages\\Bianca\\CharLobby_Bianca_S000.png");

		// 텍스처가 없다면 기본 텍스처 사용
	if (!m_lineTexture) {
		m_lineTexture = RESOURCES->Get<Texture>(L"default");
	}
	if (!m_circleTexture) {
		m_circleTexture = RESOURCES->Get<Texture>(L"default");
	}
	if (!m_coneTexture) {
		m_coneTexture = RESOURCES->Get<Texture>(L"default");
	}
	if (!m_rectangleTexture) {
		m_rectangleTexture = RESOURCES->Get<Texture>(L"default");
	}

}

void SkillDecalIndicator::UpdateLineDecal()
{
	Vec3 direction = m_targetPos - m_startPos;
	direction.y = 0;  // 바닥에 평행하게

	float distance = min(direction.Length(), m_range);
	if (distance < 0.1f) return;

	direction.Normalize();

	// 데칼 중심 위치 (선분의 중점)
	m_centerPos = m_startPos + direction * (distance * 0.5f);
	m_centerPos.y += 0.05f;  // 바닥에서 살짝 위로

	// 데칼 크기 설정
	Vec3 decalSize(distance, 0.1f, m_width);

	// 데칼 회전 (방향에 맞게)
	float angle = atan2(direction.z, direction.x);
	Vec3 rotation(0, angle, 0);

	// 데칼 변환 행렬 계산
	Matrix decalMatrix = CalculateDecalTransform(m_centerPos, decalSize, rotation);
	Matrix invDecalMatrix = decalMatrix.Invert();

	// DecalBufferData 설정
	m_decalData.decalMatrix = decalMatrix;
	m_decalData.invDecalMatrix = invDecalMatrix;
	m_decalData.decalColor = m_color;
	m_decalData.decalAlpha = m_color.w;
	m_decalData.padding = Vec3::Zero;

	// GameObject 위치 설정
	GetGameObject()->GetTransform()->SetPosition(m_centerPos);
	GetGameObject()->GetTransform()->SetRotation(Vec3(0, angle, 0));
	GetGameObject()->GetTransform()->SetScale(decalSize);

	// 머티리얼에 데칼 데이터 설정
	if (m_decalMaterial) {
		m_decalMaterial->SetDecalData(m_decalData);
	}
}

void SkillDecalIndicator::UpdateCircleDecal()
{
	m_centerPos = m_startPos;
	m_centerPos.y += 0.05f;

	Vec3 decalSize(m_range * 2.0f, 0.1f, m_range * 2.0f);
	Vec3 rotation(0, 0, 0);
	// 데칼 변환 행렬 계산
	Matrix decalMatrix = CalculateDecalTransform(m_centerPos, decalSize, rotation);
	Matrix invDecalMatrix = decalMatrix.Invert();

	// DecalBufferData 설정
	m_decalData.decalMatrix = decalMatrix;
	m_decalData.invDecalMatrix = invDecalMatrix;
	m_decalData.decalColor = m_color;
	m_decalData.decalAlpha = m_color.w;
	m_decalData.padding = Vec3::Zero;

	// GameObject 위치 설정
	GetGameObject()->GetTransform()->SetPosition(m_centerPos);
	GetGameObject()->GetTransform()->SetRotation(rotation);
	GetGameObject()->GetTransform()->SetScale(decalSize);

	// 머티리얼에 데칼 데이터 설정
	if (m_decalMaterial) {
		m_decalMaterial->SetDecalData(m_decalData);
	}
}

void SkillDecalIndicator::UpdateConeDecal()
{
	Vec3 direction = m_targetPos - m_startPos;
	direction.y = 0;
	direction.Normalize();

	// 부채꼴의 중심점
	m_centerPos = m_startPos + direction * (m_range * 0.5f);
	m_centerPos.y += 0.05f;

	Vec3 decalSize(m_range, 0.1f, m_width);

	// 방향에 맞는 회전
	float angle = atan2(direction.z, direction.x);
	Vec3 rotation(0, angle, 0);

	// 데칼 변환 행렬 계산
	Matrix decalMatrix = CalculateDecalTransform(m_centerPos, decalSize, rotation);
	Matrix invDecalMatrix = decalMatrix.Invert();

	// DecalBufferData 설정
	m_decalData.decalMatrix = decalMatrix;
	m_decalData.invDecalMatrix = invDecalMatrix;
	m_decalData.decalColor = m_color;
	m_decalData.decalAlpha = m_color.w;
	m_decalData.padding = Vec3::Zero;

	// GameObject 위치 설정
	GetGameObject()->GetTransform()->SetPosition(m_centerPos);
	GetGameObject()->GetTransform()->SetRotation(rotation);
	GetGameObject()->GetTransform()->SetScale(decalSize);

	// 머티리얼에 데칼 데이터 설정
	if (m_decalMaterial) {
		m_decalMaterial->SetDecalData(m_decalData);
	}
}

void SkillDecalIndicator::UpdateRectangleDecal()
{
	Vec3 direction = m_targetPos - m_startPos;
	direction.y = 0;

	float distance = min(direction.Length(), m_range);
	if (distance < 0.1f) return;

	direction.Normalize();

	// 사각형 중심
	m_centerPos = m_startPos + direction * (distance * 0.5f);
	m_centerPos.y += 0.05f;

	Vec3 decalSize(distance, 0.1f, m_width);

	float angle = atan2(direction.z, direction.x);
	Vec3 rotation(0, angle, 0);

	// 데칼 변환 행렬 계산
	Matrix decalMatrix = CalculateDecalTransform(m_centerPos, decalSize, rotation);
	Matrix invDecalMatrix = decalMatrix.Invert();

	// DecalBufferData 설정
	m_decalData.decalMatrix = decalMatrix;
	m_decalData.invDecalMatrix = invDecalMatrix;
	m_decalData.decalColor = m_color;
	m_decalData.decalAlpha = m_color.w;
	m_decalData.padding = Vec3::Zero;

	// GameObject 위치 설정
	GetGameObject()->GetTransform()->SetPosition(m_centerPos);
	GetGameObject()->GetTransform()->SetRotation(rotation);
	GetGameObject()->GetTransform()->SetScale(decalSize);

	// 머티리얼에 데칼 데이터 설정
	if (m_decalMaterial) {
		m_decalMaterial->SetDecalData(m_decalData);
	}
}

Vec3 SkillDecalIndicator::GetMouseWorldPostion()
{
	POINT mousePos = INPUT->GetMousePos();
	Vec2 screenPos(mousePos.x, mousePos.y);

	// 메인 카메라 가져오기
	auto camera = CURSCENE->GetMainCamera()->GetCamera();

	// 마우스에서 레이 생성
	Ray ray = CURSCENE->GetObjectManager()->CreateRayFromScreen(screenPos, camera);

	// 바닥 평면(Y=0)과의 교점 계산
	float t = -ray.position.y / ray.direction.y;
	if (t > 0) {
		return ray.position + ray.direction * t;
	}

	return Vec3::Zero;
}

Matrix SkillDecalIndicator::CalculateDecalTransform(const Vec3& _center, const Vec3& _size, const Vec3& _rotation)
{
	Matrix scaleMatrix = Matrix::CreateScale(_size);
	Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(_rotation.y, _rotation.x, _rotation.z);
	Matrix translationMatrix = Matrix::CreateTranslation(_center);

	//return translationMatrix * rotationMatrix * scaleMatrix;
	return scaleMatrix * rotationMatrix * translationMatrix;
}
