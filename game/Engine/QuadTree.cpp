#include "pch.h"
#include "QuadTree.h"
#include "GameObject.h"
#include "Transform.h"
#include "Camera.h"
#include "BaseCollider.h"
#include "SphereCollider.h"
#include "AABBBoxCollider.h"

int QuadTree::s_nextNodeId = 0;


QuadTreeNode::~QuadTreeNode() {
	objects.clear();

	for (int i = 0; i < 4; ++i) {
		if (children[i])
			children[i].reset();
	}
}

QuadTree::QuadTree(float _screenWidth, float _screenHeight)
	: m_screenWidth(_screenWidth), m_screenHeight(_screenHeight)
{
	m_root = make_unique<QuadTreeNode>();
	m_root->bounds = { 0, 0, (LONG)m_screenWidth, (LONG)m_screenHeight };
	m_root->nodeId = s_nextNodeId++;
}

QuadTree::~QuadTree()
{
	Clear();
}

void QuadTree::Clear()
{
	if (m_root) {
		m_root->objects.clear();
		for (int idx = 0; idx < 4; ++idx) {
			m_root->children[idx].reset();
		}
		m_root->isLeaf = true;
	}
	m_insertedObjects.clear();
	m_stats = QuadTreeStats{};
}

void QuadTree::Insert(shared_ptr<GameObject> _object)
{
	if (!_object || !_object->GetCollider() || !_object->GetActive()) return;

	//중복 검사. 
	if (m_insertedObjects.find(_object) != m_insertedObjects.end())
		return;

	InsertIntoNode(m_root, _object, 0);
	m_insertedObjects.insert(_object);
}

void QuadTree::Build()
{
#ifndef _DEBUG
	auto start = std::chrono::high_resolution_clock::now();
#endif 

	//Insert과정에서 이미 트리가 구성되므로 통계만 업데이트.
	UpdateStats();

#ifndef _DEBUG
	auto end = std::chrono::high_resolution_clock::now();
	m_stats.lastBuildTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
#endif
}

vector<shared_ptr<GameObject>> QuadTree::Query(const Ray& _ray, shared_ptr<Camera> _camera)
{
#ifndef _DEBUG
	auto start = std::chrono::high_resolution_clock::now();
#endif

	vector<shared_ptr<GameObject>> result;
	QueryNode(m_root, _ray, _camera, result);

	//중복 제거
	sort(result.begin(), result.end());
	result.erase(unique(result.begin(), result.end()), result.end());

#ifndef _DEBUG
	auto end = std::chrono::high_resolution_clock::now();
	m_stats.lastQueryTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
#endif
	return result;
}

void QuadTree::UpdateStats()
{
	m_stats = QuadTreeStats{};
	if (m_root)
	{
		CalculateStats(m_root, 0);
		if (m_stats.leafNodes > 0)
		{
			m_stats.avgObjectsPerLeaf = (float)m_stats.totalObjects / m_stats.leafNodes;
		}
	}
}

void QuadTree::DebugDraw(shared_ptr<Camera> _camera)
{
	if (!m_root) return;

	cout << "=== QuadTree Debug Draw ===" << endl;
	cout << "Screen Size: " << m_screenWidth << " x " << m_screenHeight << endl;
	cout << "Performance Stats:" << endl;
	cout << "  Total Nodes: " << m_stats.totalNodes << endl;
	cout << "  Leaf Nodes: " << m_stats.leafNodes << endl;
	cout << "  Max Depth: " << m_stats.maxDepth << endl;
	cout << "  Avg Objects/Leaf: " << m_stats.avgObjectsPerLeaf << endl;
	cout << "  Last Query Time: " << m_stats.lastQueryTime.count() << "μs" << endl;
	cout << "  Last Build Time: " << m_stats.lastBuildTime.count() << "μs" << endl;

	DebugDrawNode(m_root, 0, _camera);
}

void QuadTree::PrintTreeStructure()
{
	if (!m_root)
	{
		cout << "QuadTree is empty!" << endl;
		return;
	}

	cout << "=== QuadTree Structure ===" << endl;
	PrintNodeStructure(m_root, 0, "");

}

void QuadTree::PrintDuplicates()
{
	unordered_map<shared_ptr<GameObject>, int> objectCount;
	CountObjectsInNode(m_root, objectCount);

	cout << "=== 중복 객체 검사 결과 ===" << endl;
	int duplicateCount = 0;
	for (auto& pair : objectCount)
	{
		if (pair.second > 1)
		{
			wcout << L"중복 객체: " << pair.first->GetName()
				<< L" (개수: " << pair.second << L")" << endl;
			duplicateCount++;
		}
	}
	cout << "총 중복 객체 수: " << duplicateCount << endl;

}

void QuadTree::GetNodeBounds(vector<RECT>& _bounds, vector<int>& _depths)
{
	_bounds.clear();
	_depths.clear();

	if (m_root)
	{
		CollectNodeBounds(m_root, 0, _bounds, _depths);
	}
}

void QuadTree::Split(unique_ptr<QuadTreeNode>& _node, int _depth)
{
	if (!_node || !_node->isLeaf || _depth >= QuadTreeNode::MAX_DEPTH) return;

	LONG halfWidth = (_node->bounds.right - _node->bounds.left) / 2;
	LONG halfHeight = (_node->bounds.bottom - _node->bounds.top) / 2;
	LONG x = _node->bounds.left;
	LONG y = _node->bounds.top;

	//자식 노드(쿼드 노드) 생성. 
	for (int i = 0; i < 4; ++i) {
		_node->children[i] = make_unique<QuadTreeNode>();
		_node->children[i]->nodeId = s_nextNodeId++;
	}

	//경계 설정.
	_node->children[0]->bounds = { x, y, x + halfWidth, y + halfHeight }; // 좌상단
	_node->children[1]->bounds = { x + halfWidth, y, x + halfWidth * 2, y + halfHeight }; // 우상단
	_node->children[2]->bounds = { x, y + halfHeight, x + halfWidth, y + halfHeight * 2 }; // 좌하단
	_node->children[3]->bounds = { x + halfWidth, y + halfHeight, x + halfWidth * 2, y + halfHeight * 2 }; // 우하단

	_node->isLeaf = false;

	//기존 객체들을 자식으로 재분배(겹쳐있을 경우 여러 노드에 중복 삽입 허용)
	vector<shared_ptr<GameObject>> tempObjects = move(_node->objects);
	_node->objects.clear();

	for (auto& obj : tempObjects) {
		shared_ptr<Camera> camera = CURSCENE->GetMainCamera()->GetCamera();
		RECT objBounds = GetObjectScreenBounds(obj, camera);

		//모든 자식 노드와 교차 검사. 이후, 해당하는 모든 노드에 삽입. 
		bool insertedIntoChild = false;
		for (int i = 0; i < 4; ++i) {
			RECT intersection;
			if (IntersectRect(&intersection, &objBounds, &_node->children[i]->bounds)) {
				InsertIntoNode(_node->children[i], obj, _depth + 1);
				insertedIntoChild = true;
			}
		}

		//만약 어떤 자식 노드와도 교체하지 않으면 현재 노드에 보관
		if (!insertedIntoChild) {
			_node->objects.push_back(obj);
		}
	}

}

void QuadTree::InsertIntoNode(unique_ptr<QuadTreeNode>& _node, shared_ptr<GameObject> _object, int _depth)
{
	if (!_node) return;

	if (_node->isLeaf) {
		//이미 존재하는지 확인(중복 방지)
		auto it = find(_node->objects.begin(), _node->objects.end(), _object);
		if (it == _node->objects.end()) {
			_node->objects.push_back(_object);
		}

		//분할 조건 확인
		if (_node->objects.size() > QuadTreeNode::MAX_OBJECTS && _depth < QuadTreeNode::MAX_DEPTH) {
			Split(_node, _depth);
		}
	}
	else {
		//객체가 교차하는 모든 자식 노드에 삽입. 

		shared_ptr<Camera> camera = CURSCENE->GetMainCamera()->GetCamera();
		RECT objBounds = GetObjectScreenBounds(_object, camera);

		bool insertedIntoChild = false;
		for (int i = 0; i < 4; ++i) {
			RECT intersection;
			if (IntersectRect(&intersection, &objBounds, &_node->children[i]->bounds)) {
				InsertIntoNode(_node->children[i], _object, _depth + 1);
				insertedIntoChild = true;
			}
		}

		//어떤 자식 노드와도 교차하지 않으면 현재 노드에 보관
		if (!insertedIntoChild) {
			auto it = find(_node->objects.begin(), _node->objects.end(), _object);
			if (it == _node->objects.end()) {
				_node->objects.push_back(_object);
			}
		}
	}
}

void QuadTree::QueryNode(const unique_ptr<QuadTreeNode>& _node, const Ray& _ray, shared_ptr<Camera> _camera, vector<shared_ptr<GameObject>>& _result)
{
	if (!_node) return;

	//Ray가 이 노드의 영역과 교차하는지 확인
	if (!RayIntersectsAABB(_ray, _node->bounds, _camera)) return;

	for (auto& obj : _node->objects) {
		if (IsObjectVisible(obj, _camera)) {
			_result.push_back(obj);
		}
	}

	//자식 노드들을 재귀적으로 검사
	if (!_node->isLeaf) {
		for (int i = 0; i < 4; ++i) {
			QueryNode(_node->children[i], _ray, _camera, _result);
		}
	}
}

bool QuadTree::RayIntersectsAABB(const Ray& _ray, const RECT& _rect, shared_ptr<Camera> _camera)
{
	// 1. Ray의 시작점과 끝점을 화면 좌표로 변환
	Vec2 rayStart = WorldToScreen(_ray.position, _camera);
	Vec2 rayEnd = WorldToScreen(_ray.position + _ray.direction * 1000.0f, _camera); // 충분히 먼 거리

	// 2. 화면 밖 Ray 제외
	Viewport viewport = GRAPHICS->GetViewport();
	if ((rayStart.x < -1000 && rayEnd.x < -1000) ||
		(rayStart.x > viewport.GetWidth() + 1000 && rayEnd.x > viewport.GetWidth() + 1000) ||
		(rayStart.y < -1000 && rayEnd.y < -1000) ||
		(rayStart.y > viewport.GetHeight() + 1000 && rayEnd.y > viewport.GetHeight() + 1000))
	{
		return false;
	}

	// 3. 2D 선분-사각형 교차 검사
	return LineIntersectsRect(rayStart, rayEnd, _rect);
}

//2D 선분-사각형 교차 검사 함수. 
bool QuadTree::LineIntersectsRect(const Vec2& _lineStart, const Vec2& _lineEnd, const RECT& _rect)
{
	// 사각형의 네 모서리와 선분의 교차 검사
	Vec2 rectPoints[4] = {
		Vec2(_rect.left, _rect.top),      // 좌상단
		Vec2(_rect.right, _rect.top),     // 우상단
		Vec2(_rect.right, _rect.bottom),  // 우하단
		Vec2(_rect.left, _rect.bottom)    // 좌하단
	};

	// 사각형의 네 변과 선분의 교차 검사
	for (int i = 0; i < 4; ++i)
	{
		Vec2 edgeStart = rectPoints[i];
		Vec2 edgeEnd = rectPoints[(i + 1) % 4];

		if (LineSegmentIntersect(_lineStart, _lineEnd, edgeStart, edgeEnd))
		{
			return true;
		}
	}

	// 선분의 시작점이나 끝점이 사각형 내부에 있는지 검사
	if (PointInRect(_lineStart, _rect) || PointInRect(_lineEnd, _rect))
	{
		return true;
	}

	return false;

}

//두 선분의 교차 검사. 
bool QuadTree::LineSegmentIntersect(const Vec2& _p1, const Vec2& _q1, const Vec2& _p2, const Vec2& _q2)
{
	auto orientation = [](const Vec2& p, const Vec2& q, const Vec2& r) -> int {
		float val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
		if (abs(val) < 1e-6f) return 0;  // 평행
		return (val > 0) ? 1 : 2;        // 시계방향 또는 반시계방향
	};

	auto onSegment = [](const Vec2& p, const Vec2& q, const Vec2& r) -> bool {
		return q.x <= max(p.x, r.x) && q.x >= min(p.x, r.x) &&
			q.y <= max(p.y, r.y) && q.y >= min(p.y, r.y);
	};

	int o1 = orientation(_p1, _q1, _p2);
	int o2 = orientation(_p1, _q1, _q2);
	int o3 = orientation(_p2, _q2, _p1);
	int o4 = orientation(_p2, _q2, _q1);

	// 일반적인 경우
	if (o1 != o2 && o3 != o4)
		return true;

	// 특수한 경우들
	if (o1 == 0 && onSegment(_p1, _p2, _q1)) return true;
	if (o2 == 0 && onSegment(_p1, _q2, _q1)) return true;
	if (o3 == 0 && onSegment(_p2, _p1, _q2)) return true;
	if (o4 == 0 && onSegment(_p2, _q1, _q2)) return true;

	return false;

}

bool QuadTree::PointInRect(const Vec2& _point, const RECT& _rect)
{
	return _point.x >= _rect.left && _point.x <= _rect.right &&
		_point.y >= _rect.top && _point.y <= _rect.bottom;
}

RECT QuadTree::GetObjectScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera)
{
	if (!_object->GetCollider()) {
		return { -10000, -10000, -10000, -10000 };
	}

	Vec3 objectPos = _object->GetTransform()->GetPosition();
	Vec3 cameraPos = _camera->GetTransform()->GetPosition();

	//카메라 뒤에 있는 객체 제외
	Vec3 dirToObj = objectPos - cameraPos;
	Vec3 cameraLook = _camera->GetTransform()->GetLook();


	if (dirToObj.Dot(cameraLook) < 0) {
		return { -10000, -10000, -10000, -10000 };
	}


	//콜라이더 타입별 실제 바운딩 박스 계산
	RECT screenBounds = CalculateColliderScreenBounds(_object, _camera);

	//화면 밖 객체 제외
	Viewport viewport = GRAPHICS->GetViewport();
	if (screenBounds.right < -100 || screenBounds.left > viewport.GetWidth() + 100 ||
		screenBounds.bottom < -100 || screenBounds.top > viewport.GetHeight() + 100)
	{
		return { -10000, -10000, -10000, -10000 };
	}

	return screenBounds;
}

RECT QuadTree::CalculateColliderScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera)
{
	auto collider = _object->GetCollider();
	ColliderType type = collider->GetColliderType();

	switch (type)
	{
	case ColliderType::Sphere:
		return CalculateSphereScreenBounds(_object, _camera);
	case ColliderType::AABB:
		return CalculateColliderAABBScreenBounds(_object, _camera);
	default:
		return CalculateDefaultScreenBounds(_object, _camera);
	}
}

RECT QuadTree::CalculateColliderAABBScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera)
{
	auto aabbCollider = dynamic_pointer_cast<AABBBoxCollider>(_object->GetCollider());
	if (!aabbCollider) return { -10000, -10000, -10000, -10000 };

	// AABB의 실제 BoundingBox 사용
	BoundingBox& boundingBox = aabbCollider->GetBoundingBox();
	Vec3 center = boundingBox.Center;
	Vec3 extents = boundingBox.Extents;

	// AABB의 8개 꼭짓점 계산
	vector<Vec3> corners = {
		center + Vec3(-extents.x, -extents.y, -extents.z), // 좌하후
		center + Vec3(+extents.x, -extents.y, -extents.z), // 우하후
		center + Vec3(-extents.x, +extents.y, -extents.z), // 좌상후
		center + Vec3(+extents.x, +extents.y, -extents.z), // 우상후
		center + Vec3(-extents.x, -extents.y, +extents.z), // 좌하전
		center + Vec3(+extents.x, -extents.y, +extents.z), // 우하전
		center + Vec3(-extents.x, +extents.y, +extents.z), // 좌상전
		center + Vec3(+extents.x, +extents.y, +extents.z)  // 우상전
	};

	// 모든 꼭짓점을 화면 좌표로 변환하여 최소/최대값 찾기
	float minX = FLT_MAX, maxX = -FLT_MAX;
	float minY = FLT_MAX, maxY = -FLT_MAX;
	bool hasValidPoint = false;

	for (const Vec3& corner : corners)
	{
		// 카메라 뒤에 있는 점 제외
		Vec3 dirToCorner = corner - _camera->GetTransform()->GetPosition();
		if (dirToCorner.Dot(_camera->GetTransform()->GetLook()) < 0)
			continue;

		Vec2 screenPos = WorldToScreen(corner, _camera);

		// 유효한 화면 좌표인지 확인
		Viewport viewport = GRAPHICS->GetViewport();
		if (screenPos.x >= -1000 && screenPos.x <= viewport.GetWidth() + 1000 &&
			screenPos.y >= -1000 && screenPos.y <= viewport.GetHeight() + 1000)
		{
			minX = min(minX, screenPos.x);
			maxX = max(maxX, screenPos.x);
			minY = min(minY, screenPos.y);
			maxY = max(maxY, screenPos.y);
			hasValidPoint = true;
		}
	}

	// 유효한 점이 없으면 무효한 영역 반환
	if (!hasValidPoint)
	{
		return { -10000, -10000, -10000, -10000 };
	}

	// 최소 크기 보장
	float minSize = 4.0f;
	if (maxX - minX < minSize)
	{
		float centerX = (minX + maxX) * 0.5f;
		minX = centerX - minSize * 0.5f;
		maxX = centerX + minSize * 0.5f;
	}
	if (maxY - minY < minSize)
	{
		float centerY = (minY + maxY) * 0.5f;
		minY = centerY - minSize * 0.5f;
		maxY = centerY + minSize * 0.5f;
	}

	return { (LONG)minX, (LONG)minY, (LONG)maxX, (LONG)maxY };
}

RECT QuadTree::CalculateSphereScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera)
{
	auto sphereCollider = dynamic_pointer_cast<SphereCollider>(_object->GetCollider());
	if (!sphereCollider) return { -10000, -10000, -10000, -10000 };

	// SphereCollider의 실제 BoundingSphere 사용
	BoundingSphere& boundingSphere = sphereCollider->GetBoundSphere();
	Vec3 sphereCenter = boundingSphere.Center;
	float radius = boundingSphere.Radius;

	// 구체의 경계점들을 계산 (더 정확한 투영을 위해 더 많은 점 사용)
	vector<Vec3> boundaryPoints;

	// 주요 축 방향 경계점들
	boundaryPoints.push_back(sphereCenter + Vec3(radius, 0, 0));    // +X
	boundaryPoints.push_back(sphereCenter + Vec3(-radius, 0, 0));   // -X
	boundaryPoints.push_back(sphereCenter + Vec3(0, radius, 0));    // +Y
	boundaryPoints.push_back(sphereCenter + Vec3(0, -radius, 0));   // -Y
	boundaryPoints.push_back(sphereCenter + Vec3(0, 0, radius));    // +Z
	boundaryPoints.push_back(sphereCenter + Vec3(0, 0, -radius));   // -Z

	// 대각선 방향 경계점들 (더 정확한 바운딩을 위해)
	float diagRadius = radius * 0.707f; // sqrt(2)/2
	boundaryPoints.push_back(sphereCenter + Vec3(diagRadius, diagRadius, 0));
	boundaryPoints.push_back(sphereCenter + Vec3(diagRadius, -diagRadius, 0));
	boundaryPoints.push_back(sphereCenter + Vec3(-diagRadius, diagRadius, 0));
	boundaryPoints.push_back(sphereCenter + Vec3(-diagRadius, -diagRadius, 0));
	boundaryPoints.push_back(sphereCenter + Vec3(diagRadius, 0, diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(diagRadius, 0, -diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(-diagRadius, 0, diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(-diagRadius, 0, -diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(0, diagRadius, diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(0, diagRadius, -diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(0, -diagRadius, diagRadius));
	boundaryPoints.push_back(sphereCenter + Vec3(0, -diagRadius, -diagRadius));

	// 모든 경계점을 화면 좌표로 변환하여 최소/최대값 찾기
	float minX = FLT_MAX, maxX = -FLT_MAX;
	float minY = FLT_MAX, maxY = -FLT_MAX;
	bool hasValidPoint = false;

	for (const Vec3& point : boundaryPoints)
	{
		// 카메라 뒤에 있는 점 제외
		Vec3 dirToPoint = point - _camera->GetTransform()->GetPosition();
		if (dirToPoint.Dot(_camera->GetTransform()->GetLook()) < 0)
			continue;

		Vec2 screenPos = WorldToScreen(point, _camera);

		// 유효한 화면 좌표인지 확인
		Viewport viewport = GRAPHICS->GetViewport();
		if (screenPos.x >= -1000 && screenPos.x <= viewport.GetWidth() + 1000 &&
			screenPos.y >= -1000 && screenPos.y <= viewport.GetHeight() + 1000)
		{
			minX = min(minX, screenPos.x);
			maxX = max(maxX, screenPos.x);
			minY = min(minY, screenPos.y);
			maxY = max(maxY, screenPos.y);
			hasValidPoint = true;
		}
	}

	// 유효한 점이 없으면 무효한 영역 반환
	if (!hasValidPoint)
	{
		return { -10000, -10000, -10000, -10000 };
	}

	// 최소 크기 보장 (너무 작은 바운딩 박스 방지)
	float minSize = 4.0f;
	if (maxX - minX < minSize)
	{
		float center = (minX + maxX) * 0.5f;
		minX = center - minSize * 0.5f;
		maxX = center + minSize * 0.5f;
	}
	if (maxY - minY < minSize)
	{
		float center = (minY + maxY) * 0.5f;
		minY = center - minSize * 0.5f;
		maxY = center + minSize * 0.5f;
	}

	return { (LONG)minX, (LONG)minY, (LONG)maxX, (LONG)maxY };

}

RECT QuadTree::CalculateDefaultScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera)
{
	return RECT();
}

bool QuadTree::IsObjectVisible(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera)
{
	Vec3 worldPos = _object->GetTransform()->GetPosition();
	Vec3 cameraPos = _camera->GetTransform()->GetPosition();

	//1. 거리 기반 컬링
	float distance = Vec3::Distance(worldPos, cameraPos);
	if (distance > 500.f) return false;

	//2. 백페이스 컬링(카메리 뒤쪽)
	Vec3 dirToObj = worldPos - cameraPos;
	dirToObj.Normalize();
	Vec3 cameraLook = _camera->GetTransform()->GetLook();

	float dot = dirToObj.Dot(cameraLook);
	if (dot < -0.1f) return false;

	// 3. 추가 : 화면 투영 검사. 
	Viewport viewport = GRAPHICS->GetViewport();
	Matrix worldMatrix = Matrix::Identity;
	Matrix viewMatrix = _camera->GetViewMatrix();
	Matrix projMatrix = _camera->GetProjectionMatrix();

	Vec3 screenPos = viewport.Project(worldPos, worldMatrix, viewMatrix, projMatrix);

	//화면 경계 검사. (여유 공간 최소화)
	float margin = 150.0f; // 여유 공간 줄임
	if (screenPos.x < -margin || screenPos.x > viewport.GetWidth() + margin ||
		screenPos.y < -margin || screenPos.y > viewport.GetHeight() + margin ||
		screenPos.z < 0 || screenPos.z > 1)
	{
		return false;
	}

	//FOV 기반 시야각 검사. 
	Vec3 cameraRight = _camera->GetTransform()->GetRight();
	Vec3 cameraUp = _camera->GetTransform()->GetUp();

	// 카메라 기준 로컬 좌표로 변환
	Vec3 localPos = worldPos - cameraPos;
	float forward = localPos.Dot(cameraLook);
	float right = localPos.Dot(cameraRight);
	float up = localPos.Dot(cameraUp);

	//if (forward <= 0.05f) return false; // 너무 가까운 객체 제외

	// FOV 기반 시야각 검사
	float fov = XMConvertToDegrees(_camera->GetFOV());
	float aspectRatio = viewport.GetWidth() / viewport.GetHeight();

	float horizontalFOV = fov * aspectRatio;
	float verticalFOV = fov;

	float horizontalAngle = XMConvertToDegrees(atan2(abs(right), forward));
	float verticalAngle = XMConvertToDegrees(atan2(abs(up), forward));

	if (horizontalAngle > horizontalFOV / 2.0f + 20.0f || // 10도 여유
		verticalAngle > verticalFOV / 2.0f + 20.0f)
	{
		return false;
	}

	return true;

}

bool QuadTree::IsObjectVisible(shared_ptr<GameObject> _object, Camera* _camera)
{
	//MAP오브젝트면 무조건 보이도록. 

	Vec3 worldPos = _object->GetTransform()->GetPosition();
	Vec3 cameraPos = _camera->GetTransform()->GetPosition();

	//1. 거리 기반 컬링
	float distance = Vec3::Distance(worldPos, cameraPos);
	if (distance > 500.f) return false;

	//2. 백페이스 컬링(카메리 뒤쪽)
	Vec3 dirToObj = worldPos - cameraPos;
	dirToObj.Normalize();
	Vec3 cameraLook = _camera->GetTransform()->GetLook();

	float dot = dirToObj.Dot(cameraLook);
	if (dot < -0.2f) return false;

	// 3. 추가 : 화면 투영 검사. 
	Viewport viewport = GRAPHICS->GetViewport();
	Matrix worldMatrix = Matrix::Identity;
	Matrix viewMatrix = _camera->GetViewMatrix();
	Matrix projMatrix = _camera->GetProjectionMatrix();

	Vec3 screenPos = viewport.Project(worldPos, worldMatrix, viewMatrix, projMatrix);

	//화면 경계 검사. (여유 공간 최소화)
	float margin = 250.0f; // 여유 공간 줄임
	if (screenPos.x < -margin || screenPos.x > viewport.GetWidth() + margin ||
		screenPos.y < -margin || screenPos.y > viewport.GetHeight() + margin ||
		screenPos.z < 0 || screenPos.z > 1)
	{
		return false;
	}

	//FOV 기반 시야각 검사. 
	Vec3 cameraRight = _camera->GetTransform()->GetRight();
	Vec3 cameraUp = _camera->GetTransform()->GetUp();

	// 카메라 기준 로컬 좌표로 변환
	Vec3 localPos = worldPos - cameraPos;
	float forward = localPos.Dot(cameraLook);
	float right = localPos.Dot(cameraRight);
	float up = localPos.Dot(cameraUp);

	if (forward <= 0.03f) return false; // 너무 가까운 객체 제외

	// FOV 기반 시야각 검사
	float fov = XMConvertToDegrees(_camera->GetFOV());
	float aspectRatio = viewport.GetWidth() / viewport.GetHeight();

	float horizontalFOV = fov * aspectRatio;
	float verticalFOV = fov;

	float horizontalAngle = XMConvertToDegrees(atan2(abs(right), forward));
	float verticalAngle = XMConvertToDegrees(atan2(abs(up), forward));

	if (horizontalAngle > horizontalFOV / 2.0f + 30.0f || // 15도 여유
		verticalAngle > verticalFOV / 2.0f + 30.0f)
	{
		return false;
	}

	return true;
}


void QuadTree::DebugDrawNode(const unique_ptr<QuadTreeNode>& _node, int _depth, shared_ptr<Camera> _camera)
{
	if (!_node) return;

	string indent(_depth * 2, ' ');
	cout << indent << "Node " << _node->nodeId << " [D" << _depth << "]: ";
	cout << "Bounds(" << _node->bounds.left << "," << _node->bounds.top << ","
		<< _node->bounds.right << "," << _node->bounds.bottom << ") ";
	cout << "Objects: " << _node->objects.size();

	if (_node->isLeaf)
	{
		cout << " [LEAF]";
	}
	else
	{
		cout << " [BRANCH]";
	}
	cout << endl;

	if (!_node->objects.empty())
	{
		for (auto& obj : _node->objects)
		{
			cout << indent << "  - " << ws2s(obj->GetName()) << endl;
		}
	}

	if (!_node->isLeaf)
	{
		for (int i = 0; i < 4; ++i)
		{
			DebugDrawNode(_node->children[i], _depth + 1, _camera);
		}
	}

}

void QuadTree::PrintNodeStructure(const unique_ptr<QuadTreeNode>& _node, int _depth, const string& _prefix)
{
	if (!_node) return;

	cout << _prefix;
	cout << "├─ [N" << _node->nodeId << "|D" << _depth << "] ";
	cout << "(" << _node->bounds.left << "," << _node->bounds.top << ","
		<< _node->bounds.right << "," << _node->bounds.bottom << ") ";
	cout << "Objs:" << _node->objects.size();

	if (_node->isLeaf)
	{
		cout << " LEAF";
	}
	cout << endl;

	if (!_node->isLeaf)
	{
		string newPrefix = _prefix + "│  ";
		for (int i = 0; i < 4; ++i)
		{
			PrintNodeStructure(_node->children[i], _depth + 1, newPrefix);
		}
	}


}

void QuadTree::CollectNodeBounds(const unique_ptr<QuadTreeNode>& _node, int _depth, vector<RECT>& _bounds, vector<int>& _depths)
{
	if (!_node) return;

	_bounds.push_back(_node->bounds);
	_depths.push_back(_depth);

	if (!_node->isLeaf)
	{
		for (int i = 0; i < 4; ++i)
		{
			CollectNodeBounds(_node->children[i], _depth + 1, _bounds, _depths);
		}
	}
}

void QuadTree::CountObjectsInNode(const unique_ptr<QuadTreeNode>& _node, unordered_map<shared_ptr<GameObject>, int>& _objectCount)
{
	if (!_node) return;

	for (auto& obj : _node->objects)
	{
		_objectCount[obj]++;
	}

	if (!_node->isLeaf)
	{
		for (int i = 0; i < 4; ++i)
		{
			CountObjectsInNode(_node->children[i], _objectCount);
		}
	}
}

void QuadTree::CalculateStats(const unique_ptr<QuadTreeNode>& _node, int _depth)
{
	if (!_node) return;

	m_stats.totalNodes++;
	m_stats.maxDepth = max(m_stats.maxDepth, _depth);
	m_stats.totalObjects += (int)_node->objects.size();

	if (_node->isLeaf)
	{
		m_stats.leafNodes++;
	}
	else
	{
		for (int i = 0; i < 4; ++i)
		{
			CalculateStats(_node->children[i], _depth + 1);
		}
	}

}

string QuadTree::ws2s(const wstring& _wstr)
{
	string str;
	str.assign(_wstr.begin(), _wstr.end());
	return str;
}

void QuadTree::PrintObjectLocation(shared_ptr<GameObject> _targetObject)
{
	cout << "=== 객체 위치 분석: " << ws2s(_targetObject->GetName()) << " ===" << endl;

	shared_ptr<Camera> camera = CURSCENE->GetMainCamera()->GetCamera();
	RECT objBounds = GetObjectScreenBounds(_targetObject, camera);

	cout << "객체 화면 좌표: (" << objBounds.left << ", " << objBounds.top
		<< ", " << objBounds.right << ", " << objBounds.bottom << ")" << endl;

	PrintObjectInNodes(m_root, _targetObject, 0);

}

void QuadTree::PrintObjectInNodes(const unique_ptr<QuadTreeNode>& _node, shared_ptr<GameObject> _targetObject, int _depth)
{
	if (!_node) return;

	// 이 노드에 객체가 있는지 확인
	auto it = find(_node->objects.begin(), _node->objects.end(), _targetObject);
	if (it != _node->objects.end())
	{
		string indent(_depth * 2, ' ');
		cout << indent << "발견됨 - 노드 " << _node->nodeId << " [깊이 " << _depth << "] ";
		cout << "경계: (" << _node->bounds.left << ", " << _node->bounds.top
			<< ", " << _node->bounds.right << ", " << _node->bounds.bottom << ")" << endl;

		// 노드 타입 표시
		if (_depth == 1) // 루트의 자식 노드들
		{
			string quadrant = "";
			switch (_node->nodeId % 4) // 간단한 사분면 판별
			{
			case 0: quadrant = "좌상단"; break;
			case 1: quadrant = "우상단"; break;
			case 2: quadrant = "좌하단"; break;
			case 3: quadrant = "우하단"; break;
			}
			cout << indent << "  -> " << quadrant << " 사분면" << endl;
		}
	}
	// 자식 노드들 검사
	if (!_node->isLeaf)
	{
		for (int i = 0; i < 4; ++i)
		{
			PrintObjectInNodes(_node->children[i], _targetObject, _depth + 1);
		}
	}
}

Vec2 QuadTree::WorldToScreen(const Vec3& _worldPos, shared_ptr<Camera> _camera)
{
	Viewport viewport = GRAPHICS->GetViewport();
	Matrix worldMatrix = Matrix::Identity;
	Matrix viewMatrix = _camera->GetViewMatrix();
	Matrix projMatrix = _camera->GetProjectionMatrix();

	// 월드 좌표를 화면 좌표로 변환
	Vec3 screenPos = viewport.Project(_worldPos, worldMatrix, viewMatrix, projMatrix);

	return Vec2(screenPos.x, screenPos.y);
}

Vec3 QuadTree::ScreenToWorld(const Vec2& _screenPos, shared_ptr<Camera> _camera, float _depth)
{
	Viewport viewport = GRAPHICS->GetViewport();
	Matrix worldMatrix = Matrix::Identity;
	Matrix viewMatrix = _camera->GetViewMatrix();
	Matrix projMatrix = _camera->GetProjectionMatrix();

	// 화면 좌표를 월드 좌표로 변환
	Vec3 screenPos3D = Vec3(_screenPos.x, _screenPos.y, _depth);
	Vec3 worldPos = viewport.UnProject(screenPos3D, worldMatrix, viewMatrix, projMatrix);

	return worldPos;
}

void QuadTree::DebugCoordinateTransform(const Vec2& _mousePos, shared_ptr<Camera> _camera)
{
	cout << "=== 좌표 변환 디버깅 ===" << endl;
	cout << "마우스 좌표: (" << _mousePos.x << ", " << _mousePos.y << ")" << endl;

	// 마우스 위치의 월드 좌표 계산
	Vec3 worldPos = ScreenToWorld(_mousePos, _camera, 0.5f);
	cout << "마우스 월드 좌표: (" << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << endl;

	// 다시 화면 좌표로 변환
	Vec2 backToScreen = WorldToScreen(worldPos, _camera);
	cout << "역변환 화면 좌표: (" << backToScreen.x << ", " << backToScreen.y << ")" << endl;

	float error = sqrt(pow(_mousePos.x - backToScreen.x, 2) + pow(_mousePos.y - backToScreen.y, 2));
	cout << "변환 오차: " << error << " 픽셀" << endl;
}

void QuadTree::CheckCollisionsInTree(shared_ptr<Camera> _camera, unordered_map<ULONG64, bool>& _collisionMap)
{
	if (!m_root) return;

	unordered_set<ULONG64> processedPairs;

	//노드 내부 충돌
	CheckCollisionsInNode(m_root, _collisionMap, processedPairs);
	//노드 경계 충돌. 
	CheckBoundaryCollisions(m_root, _collisionMap, processedPairs);
    m_stats.lastCollisionPairs = processedPairs.size();

}

void QuadTree::CheckCollisionsInNode(const unique_ptr<QuadTreeNode>& _node, unordered_map<ULONG64, bool>& _collisionMap, unordered_set<ULONG64>& _processedPairs)
{
	if (!_node) return;

	// 현재 노드의 객체들끼리 충돌 검사
	//노드 안에서, 한 노드에 최대 12개 MAX = 12 * 12 / 2 = 72
	auto& objects = _node->objects;
	for (size_t i = 0; i < objects.size(); ++i) {
		if (!objects[i]->GetCollider())
			continue;

		for (size_t j = i + 1; j < objects.size(); ++j) {
			if (!objects[j]->GetCollider())
				continue;

			ProcessCollisionPair(objects[i]->GetCollider(),
				objects[j]->GetCollider(),
				_collisionMap, _processedPairs);
		}
	}

	// 자식 노드들 재귀적으로 검사
	if (!_node->isLeaf) {
		for (int i = 0; i < 4; ++i) {
			CheckCollisionsInNode(_node->children[i], _collisionMap, _processedPairs);
		}

		// 인접한 자식 노드들 간의 교차 충돌 검사
		for (int i = 0; i < 4; ++i) {
			for (int j = i + 1; j < 4; ++j) {
				CheckCrossNodeCollisions(_node->children[i], _node->children[j],
					_collisionMap, _processedPairs);
			}
		}
	}
}

void QuadTree::CheckCrossNodeCollisions(const unique_ptr<QuadTreeNode>& _node1, const unique_ptr<QuadTreeNode>& _node2, unordered_map<ULONG64, bool>& _collisionMap, unordered_set<ULONG64>& _processedPairs)
{
	if (!_node1 || !_node2) return;

	// 두 노드가 인접한지 확인 (경계가 닿아있는지)
	if (!AreNodesAdjacent(_node1->bounds, _node2->bounds)) return;

	// 두 노드의 모든 객체 조합 검사
	for (auto& obj1 : _node1->objects) {
		if (!obj1->GetCollider()) continue;

		for (auto& obj2 : _node2->objects) {
			if (!obj2->GetCollider()) continue;
			if (obj1 == obj2) continue;

			ProcessCollisionPair(obj1->GetCollider(), obj2->GetCollider(),
				_collisionMap, _processedPairs);
		}
	}
}

void QuadTree::CheckBoundaryCollisions(const unique_ptr<QuadTreeNode>& _node, unordered_map<ULONG64, bool>& _collisionMap, unordered_set<ULONG64>& _processedPairs)
{
	if (!_node || _node->isLeaf) return;

	// 경계를 넘나드는 큰 객체들을 찾아서 자식 노드들과 교차 검사
	for (auto& obj : _node->objects) {
		if (!obj->GetCollider()) continue;

		// 이 객체와 모든 자식 노드의 객체들 검사
		for (int i = 0; i < 4; ++i) {
			CheckObjectWithNode(obj, _node->children[i], _collisionMap, _processedPairs);
		}
	}

	// 자식 노드들에 대해서도 재귀적으로 실행
	for (int i = 0; i < 4; ++i) {
		CheckBoundaryCollisions(_node->children[i], _collisionMap, _processedPairs);
	}
}

void QuadTree::CheckObjectWithNode(shared_ptr<GameObject> _object, const unique_ptr<QuadTreeNode>& _node, unordered_map<ULONG64, bool>& _collisionMap, unordered_set<ULONG64>& _processedPairs)
{
	if (!_node || !_object->GetCollider()) return;

	// 노드의 모든 객체와 충돌 검사
	for (auto& nodeObj : _node->objects) {
		if (!nodeObj->GetCollider()) continue;
		if (nodeObj == _object) continue; // 자기 자신 제외

		ProcessCollisionPair(_object->GetCollider(), nodeObj->GetCollider(),
			_collisionMap, _processedPairs);
	}

	// 자식 노드들과도 검사
	if (!_node->isLeaf) {
		for (int i = 0; i < 4; ++i) {
			CheckObjectWithNode(_object, _node->children[i], _collisionMap, _processedPairs);
		}
	}
}

bool QuadTree::AreNodesAdjacent(const RECT& _rect1, const RECT& _rect2)
{
	// 두 사각형이 인접한지 확인 (경계가 닿거나 겹치는지)
	return !(_rect1.right < _rect2.left || _rect2.right < _rect1.left ||
		_rect1.bottom < _rect2.top || _rect2.bottom < _rect1.top);
}

void QuadTree::ProcessCollisionPair(shared_ptr<BaseCollider> _collider1, shared_ptr<BaseCollider> _collider2, unordered_map<ULONG64, bool>& _collisionMap, unordered_set<ULONG64>& _processedPairs)
{
	//실체 충돌 처리가 이루어짐. 
	
	// ID 정렬하여 중복 방지
	COLLIDER_ID id;
	if (_collider1->GetID() < _collider2->GetID()) {
		id.left_id = _collider1->GetID();
		id.right_id = _collider2->GetID();
	}
	else {
		id.left_id = _collider2->GetID();
		id.right_id = _collider1->GetID();
	}

	// 이미 처리된 쌍인지 확인
	if (_processedPairs.find(id.ID) != _processedPairs.end()) return;
	_processedPairs.insert(id.ID);

	// 충돌 상태 확인
	auto colliderMapIter = _collisionMap.find(id.ID);
	if (colliderMapIter == _collisionMap.end()) {
		_collisionMap.insert(make_pair(id.ID, false));
		colliderMapIter = _collisionMap.find(id.ID);
	}

	// 실제 충돌 검사 및 이벤트 처리
	if (_collider1->Intersects(_collider2)) {
		if (colliderMapIter->second == false) {
			// 새로운 충돌
			_collider1->GetGameObject()->OnCollisionEnter(_collider2->GetGameObject());
			_collider2->GetGameObject()->OnCollisionEnter(_collider1->GetGameObject());
			colliderMapIter->second = true;
		}
		else {
			// 지속적인 충돌
			_collider1->GetGameObject()->OnCollision(_collider2->GetGameObject());
			_collider2->GetGameObject()->OnCollision(_collider1->GetGameObject());
		}
	}
	else {//지금 충돌X, 이전 프레임 충돌 O
		if (colliderMapIter->second == true) {
			// 충돌 종료
			_collider1->GetGameObject()->OnCollisionExit(_collider2->GetGameObject());
			_collider2->GetGameObject()->OnCollisionExit(_collider1->GetGameObject());
			colliderMapIter->second = false;
		}
	}

}
