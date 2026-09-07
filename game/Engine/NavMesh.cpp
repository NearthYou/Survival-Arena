#include "pch.h"
#include "NavMesh.h"
#include "GameObject.h"
#include "ModelRenderer.h"
#include "Model.h"
#include "ModelMesh.h"

NavMesh::NavMesh() : Super(ComponentType::NavMesh)
{
}

NavMesh::~NavMesh()
{
}

void NavMesh::Start()
{
    Super::Start();
    LoadNavMeshData();
    InitializeSpatialGrid();
    //DebugPrintTriangles();
}

void NavMesh::Update()
{
    Super::Update();
}

void NavMesh::LoadNavMeshData()
{
    auto gameObject = GetGameObject();
    if (!gameObject) return;

    auto modelRenderer = gameObject->GetModelRenderer();
    if (!modelRenderer) return;

    m_navMeshModel = modelRenderer->GetModel();
    if (!m_navMeshModel) return;

    const auto& meshes = m_navMeshModel->GetMeshes();
    auto transform = gameObject->GetTransform();
    Matrix worldMatrix = transform->GetWorldMatrix();

    // 각 메시에서 삼각형 데이터 추출
    for (size_t meshIdx = 0; meshIdx < meshes.size(); meshIdx++)
    {
        auto& mesh = meshes[meshIdx];
        auto geometry = mesh->m_geometry;
        if (!geometry) continue;

        const auto& vertices = geometry->GetVertices();
        const auto& indices = geometry->GetIndices();
        if (vertices.empty() || indices.empty()) continue;

        // 인덱스 3개씩 처리하여 삼각형 생성
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            if (indices[i] >= vertices.size() ||
                indices[i + 1] >= vertices.size() ||
                indices[i + 2] >= vertices.size()) continue;

            NavMeshTriangle triangle;

            // 월드 좌표로 변환
            for (int j = 0; j < 3; j++)
            {
                uint32 index = indices[i + j];
                Vec4 localPos = Vec4(vertices[index].position.x, vertices[index].position.y,
                    vertices[index].position.z, 1.0f);
                Vec4 worldPos = XMVector4Transform(localPos, worldMatrix);
                triangle.vertices[j] = Vec3(worldPos.x, worldPos.y, worldPos.z);
            }

            // 삼각형 유효성 검사
            Vec3 edge1 = triangle.vertices[1] - triangle.vertices[0];
            Vec3 edge2 = triangle.vertices[2] - triangle.vertices[0];
            triangle.normal = edge1.Cross(edge2);

            if (triangle.normal.Length() < 0.001f) continue;

            triangle.normal.Normalize();
            triangle.center = (triangle.vertices[0] + triangle.vertices[1] + triangle.vertices[2]) / 3.0f;
            m_triangles.push_back(triangle);
        }
    }

    if (!m_triangles.empty())
    {
        BuildTriangleConnections();
    }
}

Vec3 NavMesh::GetNearestPointOnNavMesh(const Vec3& worldPos)
{
    if (m_triangles.empty()) return worldPos;

    Vec3 nearestPoint = worldPos;
    float minDistance = FLT_MAX;

    auto key = m_spatialGrid.GetKey(worldPos);
    auto it = m_spatialGrid.grid.find(key);

    if (it != m_spatialGrid.grid.end()) {
        for (int idx : it->second) {
            Vec3 projectedPoint = ProjectPointOnTriangle(worldPos, m_triangles[idx]);
            float distance = GetDistance(worldPos, projectedPoint);

            if (distance < minDistance)
            {
                minDistance = distance;
                nearestPoint = projectedPoint;
            }
        }
    }
    //전수 조사 방식. 
    /*for (const auto& triangle : m_triangles)
    {
        Vec3 projectedPoint = ProjectPointOnTriangle(worldPos, triangle);
        float distance = GetDistance(worldPos, projectedPoint);

        if (distance < minDistance)
        {
            minDistance = distance;
            nearestPoint = projectedPoint;
        }
    }*/

    return nearestPoint;
}

bool NavMesh::RaycastNavMesh(const Ray& ray, Vec3& hitPoint)
{
    if (m_triangles.empty()) return false;

    float closestDistance = FLT_MAX;
    bool hit = false;

    for (const auto& triangle : m_triangles)
    {
        float distance = 0.0f;
        if (ray.Intersects(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2], distance))
        {
            if (distance > 0 && distance < closestDistance)
            {
                closestDistance = distance;
                hitPoint = ray.position + ray.direction * distance;
                hit = true;
            }
        }
    }

    return hit;
}

bool NavMesh::IsOnNavMesh(const Vec3& worldPos, float tolerance)
{
    Vec3 nearestPoint = GetNearestPointOnNavMesh(worldPos);
    return GetDistance(worldPos, nearestPoint) <= tolerance;
}

void NavMesh::FindPath(const Vec3& start, const Vec3& end, vector<Vec3>& outPath)
{
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
    m_diagnosticExpanded = 0;
#endif
    outPath.clear(); // 기존 내용 제거
    int startTriangle = FindTriangleContaining(start);
    int endTriangle = FindTriangleContaining(end);

    // NavMesh 위에 없는 점들을 가장 가까운 점으로 보정
    if (startTriangle == -1 || endTriangle == -1)
    {
        outPath.reserve(2); // 미리 메모리 할당
        outPath.push_back(GetNearestPointOnNavMesh(start));
        outPath.push_back(GetNearestPointOnNavMesh(end));
        return;
    }

    // 같은 삼각형에 있으면 직선 경로
    if (startTriangle == endTriangle)
    {
        outPath.reserve(2);
        outPath.push_back(start);
        outPath.push_back(end);
        return;
    }

    // A* 알고리즘으로 삼각형 경로 찾기
    vector<int> trianglePath;
    if (!FindTrianglePath(startTriangle, endTriangle, trianglePath))
    {
        outPath.reserve(2);
        outPath.push_back(GetNearestPointOnNavMesh(start));
        outPath.push_back(GetNearestPointOnNavMesh(end));
        return;
    }

    // 삼각형 경로를 월드 좌표 경로로 변환
    vector<Vec3> rawPath;
    ConvertTrianglePathToWorldPath(trianglePath, start, end, rawPath);
    // 스무딩 적용
    //auto stime = std::chrono::high_resolution_clock::now();
    SmoothPath(rawPath, outPath);
    //auto etime = std::chrono::high_resolution_clock::now();
    //cout << std::chrono::duration_cast<std::chrono::microseconds>(etime - stime).count() << "\n";
}


bool NavMesh::FindTrianglePath(int startTriangle, int endTriangle, vector<int>& outPath)
{
    outPath.clear();

    priority_queue<PathNode> openList;
    vector<bool> closedList(m_triangles.size(), false);
    vector<PathNode> allNodes(m_triangles.size());

    // 시작 노드 초기화
    PathNode startNode;
    startNode.triangleIndex = startTriangle;
    startNode.position = m_triangles[startTriangle].center;
    startNode.gCost = 0;
    startNode.hCost = GetDistance(m_triangles[startTriangle].center, m_triangles[endTriangle].center);
    startNode.fCost = startNode.gCost + startNode.hCost;
    startNode.parent = -1;

    openList.push(startNode);
    allNodes[startTriangle] = startNode;

    while (!openList.empty())
    {
        PathNode currentNode = openList.top();
        openList.pop();

        if (closedList[currentNode.triangleIndex]) continue;
        closedList[currentNode.triangleIndex] = true;
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
        ++m_diagnosticExpanded;
#endif

        if (currentNode.triangleIndex == endTriangle)
        {
            ReconstructPath(allNodes, endTriangle, outPath);
            return true; // 성공
        }

        // 인접 삼각형들 검사
        for (int neighborIndex : m_triangles[currentNode.triangleIndex].neighbors)
        {
            if (closedList[neighborIndex]) continue;

            float tentativeGCost = currentNode.gCost +
                GetDistance(m_triangles[currentNode.triangleIndex].center, m_triangles[neighborIndex].center);

            if (allNodes[neighborIndex].triangleIndex == -1 || tentativeGCost < allNodes[neighborIndex].gCost)
            {
                PathNode neighborNode;
                neighborNode.triangleIndex = neighborIndex;
                neighborNode.position = m_triangles[neighborIndex].center;
                neighborNode.gCost = tentativeGCost;
                neighborNode.hCost = GetDistance(m_triangles[neighborIndex].center, m_triangles[endTriangle].center);
                neighborNode.fCost = neighborNode.gCost + neighborNode.hCost;
                neighborNode.parent = currentNode.triangleIndex;

                allNodes[neighborIndex] = neighborNode;
                openList.push(neighborNode);
            }
        }
    }

    return false; // 경로를 찾을 수 없음
}

void NavMesh::ReconstructPath(const vector<PathNode>& allNodes, int endTriangle, vector<int>& outPath)
{
    outPath.clear();

    // 역방향으로 경로 구성
    vector<int> reversePath;
    int current = endTriangle;

    while (current != -1)
    {
        reversePath.push_back(current);
        current = allNodes[current].parent;
    }

    // 정방향으로 복사 (메모리 미리 할당)
    outPath.reserve(reversePath.size());
    for (auto it = reversePath.rbegin(); it != reversePath.rend(); ++it)
    {
        outPath.push_back(*it);
    }
}

void NavMesh::ConvertTrianglePathToWorldPath(const vector<int>& trianglePath, const Vec3& start, const Vec3& end, vector<Vec3>& outPath)
{
    outPath.clear();
    if (trianglePath.empty()) return;

    // 예상 크기로 메모리 미리 할당
    outPath.reserve(trianglePath.size() + 1);

    outPath.push_back(start);

    for (size_t i = 1; i < trianglePath.size() - 1; i++)
    {
        outPath.push_back(m_triangles[trianglePath[i]].center);
    }

    outPath.push_back(end);
}

void NavMesh::SmoothPath(const vector<Vec3>& originalPath, vector<Vec3>& outPath)
{
    outPath.clear();
    if (originalPath.size() <= 2)
    {
        outPath = originalPath; // 작은 경우에만 복사
        return;
    }

    // 예상 크기로 메모리 미리 할당 (원본보다 작을 것으로 예상)
    outPath.reserve(originalPath.size());
    outPath.push_back(originalPath[0]);

    const float minDist = 3.0f, maxDist = 10.0f;
    size_t cur = 0;

    while (cur + 1 < originalPath.size())
    {
        int bestNext = cur + 1;
        bool foundValidPath = false;

        Vec3 curPos = outPath.back();
        vector<size_t> candidates;
        candidates.reserve(10); // 일반적인 후보 개수 예상

        // 시야 확보된 모든 지점 수집
        for (size_t i = cur + 1; i < originalPath.size(); ++i)
        {
            bool hasLOS = HasLineOfSight(curPos, originalPath[i]);

            if (hasLOS) {
                bestNext = static_cast<int>(i);
                foundValidPath = true;
            }
        }

        // 중복 검사 제거: foundValidPath가 true면 IsLineOnNavMesh 생략
        if (!foundValidPath && !IsLineOnNavMesh(curPos, originalPath[bestNext]))
        {
            Vec3 safe = GetNearestPointOnNavMesh(originalPath[bestNext]);
            outPath.push_back(safe);
        }
        else
        {
            outPath.push_back(originalPath[bestNext]);
        }
        
        cur = bestNext;
        curPos = outPath.back();
    }

    // 마지막 목적지 추가
    outPath.push_back(originalPath.back());
}

void NavMesh::SmoothPathOri(const vector<Vec3>& originalPath, vector<Vec3>& outPath)
{
    outPath.clear();
    if (originalPath.size() <= 2)
    {
        outPath = originalPath; // 작은 경우에만 복사
        return;
    }

    // 예상 크기로 메모리 미리 할당 (원본보다 작을 것으로 예상)
    outPath.reserve(originalPath.size());
    outPath.push_back(originalPath[0]);

    const float minDist = 3.0f, maxDist = 10.0f;
    size_t cur = 0;

    while (cur + 1 < originalPath.size())
    {
        Vec3 curPos = outPath.back();
        vector<size_t> candidates;
        candidates.reserve(10); // 일반적인 후보 개수 예상

        // 시야 확보된 모든 지점 수집
        for (size_t i = cur + 1; i < originalPath.size(); ++i)
        {
            if (HasLineOfSight(curPos, originalPath[i]))
            {
                float d = GetDistance(curPos, originalPath[i]);
                if (d >= minDist && d <= maxDist)
                    candidates.push_back(i);
            }
            else break;
        }

        // 후보 중 가장 진행도가 높은(가장 뒤) 지점 선택
        size_t next = (candidates.empty() ? cur + 1 : candidates.back());

        // NavMesh 외 추돌 검사
        if (!IsLineOnNavMesh(curPos, originalPath[next]))
        {
            Vec3 safe = GetNearestPointOnNavMesh(originalPath[next]);
            outPath.push_back(safe);
        }
        else
        {
            outPath.push_back(originalPath[next]);
        }
        cur = next;
    }

    // 마지막 목적지 추가
    outPath.push_back(originalPath.back());
}

// Private 유틸리티 함수들
void NavMesh::BuildTriangleConnections()
{
    for (size_t i = 0; i < m_triangles.size(); i++)
    {
        m_triangles[i].index = static_cast<int>(i);
        m_triangles[i].neighbors.clear();

        for (size_t j = 0; j < m_triangles.size(); j++)
        {
            if (i != j && AreTrianglesAdjacent(m_triangles[i], m_triangles[j]))
            {
                m_triangles[i].neighbors.push_back(static_cast<int>(j));
            }
        }
    }
}

void NavMesh::InitializeSpatialGrid()
{
    m_spatialGrid.grid.clear();

    for (size_t i = 0; i < m_triangles.size(); ++i) {
        m_spatialGrid.AddTriangle(static_cast<int>(i), m_triangles[i]);
    }
}

bool NavMesh::AreTrianglesAdjacent(const NavMeshTriangle& tri1, const NavMeshTriangle& tri2)
{
    const float EPSILON = 0.01f;
    int sharedVertices = 0;

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (GetDistance(tri1.vertices[i], tri2.vertices[j]) < EPSILON)
            {
                sharedVertices++;
                break;
            }
        }
    }

    return sharedVertices >= 2;
}

int NavMesh::FindTriangleContaining(const Vec3& point)
{
    if (m_lastFoundTriangle != -1 && IsPointInTriangle(point, m_triangles[m_lastFoundTriangle])) {
        return m_lastFoundTriangle;
    }

    if (m_lastFoundTriangle != -1) {
        for (int neighbor : m_triangles[m_lastFoundTriangle].neighbors) {
            if (IsPointInTriangle(point, m_triangles[neighbor])) {
                m_lastFoundTriangle = neighbor;
                return neighbor;
            }
        }
    }

    uint64 key = m_spatialGrid.GetKey(point);
    auto it = m_spatialGrid.grid.find(key);

    if (it != m_spatialGrid.grid.end()) {
        for (int idx : it->second) {
            if (IsPointInTriangle(point, m_triangles[idx]))
            {
                return idx;
            }
        }
    }


    //전수 조사 방식. 
    /*for (size_t i = 0; i < m_triangles.size(); i++)
    {
        if (IsPointInTriangle(point, m_triangles[i]))
        {
            return static_cast<int>(i);
        }
    }*/
    return -1;
}

Vec3 NavMesh::ProjectPointOnTriangle(const Vec3& point, const NavMeshTriangle& triangle)
{
    Vec3 v0 = triangle.vertices[0];
    Vec3 planePoint = point - (point - v0).Dot(triangle.normal) * triangle.normal;

    if (IsPointInTriangle(planePoint, triangle))
    {
        return planePoint;
    }

    // 삼각형 외부에 있으면 가장 가까운 정점 반환
    Vec3 closestPoint = triangle.vertices[0];
    float minDist = GetDistance(point, triangle.vertices[0]);

    for (int i = 1; i < 3; i++)
    {
        float dist = GetDistance(point, triangle.vertices[i]);
        if (dist < minDist)
        {
            minDist = dist;
            closestPoint = triangle.vertices[i];
        }
    }

    return closestPoint;
}

bool NavMesh::IsPointInTriangle(const Vec3& point, const NavMeshTriangle& triangle)
{
    Vec3 v0 = triangle.vertices[2] - triangle.vertices[0];
    Vec3 v1 = triangle.vertices[1] - triangle.vertices[0];
    Vec3 v2 = point - triangle.vertices[0];

    float dot00 = v0.Dot(v0);
    float dot01 = v0.Dot(v1);
    float dot02 = v0.Dot(v2);
    float dot11 = v1.Dot(v1);
    float dot12 = v1.Dot(v2);

    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (u >= 0) && (v >= 0) && (u + v <= 1);
}

bool NavMesh::HasLineOfSight(const Vec3& start, const Vec3& end)
{
    return IsLineOnNavMesh(start, end);
}

bool NavMesh::IsLineOnNavMesh(const Vec3& start, const Vec3& end, float stepSize)
{
    // NavMeshAgent moves in XZ and resamples elevation on each step.
    // Cover the entire XZ segment with triangle interiors: sparse samples and
    // a one-unit padding can accept a corner that the agent cannot cross.
    (void)stepSize;
    vector<pair<double, double>> intervals;
    intervals.reserve(m_triangles.size());
    const double epsilon = 1e-7;
    for (const auto& triangle : m_triangles)
    {
        const auto& a = triangle.vertices[0];
        const auto& b = triangle.vertices[1];
        const auto& c = triangle.vertices[2];
        if (max(max(a.x, b.x), c.x) < min(start.x, end.x) - 0.001f ||
            min(min(a.x, b.x), c.x) > max(start.x, end.x) + 0.001f ||
            max(max(a.z, b.z), c.z) < min(start.z, end.z) - 0.001f ||
            min(min(a.z, b.z), c.z) > max(start.z, end.z) + 0.001f)
            continue;
        const double azcz = static_cast<double>(a.z) - c.z;
        const double bzcz = static_cast<double>(b.z) - c.z;
        const double axcx = static_cast<double>(a.x) - c.x;
        const double cxbx = static_cast<double>(c.x) - b.x;
        const double denominator = bzcz * axcx + cxbx * azcz;
        if (abs(denominator) < 1e-12) continue;
        const auto weightA = [&](const Vec3& point) {
            return (bzcz * (static_cast<double>(point.x) - c.x) + cxbx * (static_cast<double>(point.z) - c.z)) / denominator;
        };
        const auto weightB = [&](const Vec3& point) {
            return (-azcz * (static_cast<double>(point.x) - c.x) + axcx * (static_cast<double>(point.z) - c.z)) / denominator;
        };
        const double startA = weightA(start), endA = weightA(end);
        const double startB = weightB(start), endB = weightB(end);
        double first = 0, last = 1;
        const auto clip = [&](double initial, double final) {
            const double change = final - initial;
            if (abs(change) < 1e-12) return initial >= -epsilon;
            const double crossing = (-epsilon - initial) / change;
            if (change > 0) first = max(first, crossing);
            else last = min(last, crossing);
            return first <= last;
        };
        if (clip(startA, endA) && clip(startB, endB) && clip(1 - startA - startB, 1 - endA - endB))
            intervals.emplace_back(first, last);
    }
    sort(intervals.begin(), intervals.end());
    double covered = 0;
    for (const auto& interval : intervals)
    {
        if (interval.first > covered + epsilon) return false;
        covered = max(covered, interval.second);
        if (covered >= 1 - epsilon) return true;
    }
    return false;
}

float NavMesh::GetDistance(const Vec3& a, const Vec3& b)
{
    return Vec3::Distance(a, b);
}

int NavMesh::GetTotalNeighbors() const
{
    int total = 0;
    for (const auto& tri : m_triangles)
    {
        total += static_cast<int>(tri.neighbors.size());
    }
    return total;
}

void NavMesh::DebugPrintTriangles()
{
    cout << "=== NavMesh Debug Info ===" << endl;
    cout << "Total triangles: " << m_triangles.size() << endl;
    if (!m_triangles.empty())
    {
        cout << "Average neighbors per triangle: "
            << static_cast<float>(GetTotalNeighbors()) / m_triangles.size() << endl;
    }
}