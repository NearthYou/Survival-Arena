#pragma once
#include "Component.h"
#include <queue>

// 삼각형 구조체
struct NavMeshTriangle {
    Vec3 vertices[3];
    Vec3 normal;
    Vec3 center;
    vector<int> neighbors;
    int index;

    NavMeshTriangle() : index(-1) {}
};

// A* 경로찾기용 노드
struct PathNode {
    int triangleIndex;
    Vec3 position;
    float gCost, hCost, fCost;
    int parent;

    PathNode() : triangleIndex(-1), gCost(0), hCost(0), fCost(0), parent(-1) {}

    bool operator<(const PathNode& other) const {
        return fCost > other.fCost;
    }
};

//Grid 기반 공간 분할
struct SpatialGrid {
    float cellsize = 25.f;
    unordered_map<uint64, vector<int>> grid;

    uint64 GetKey(const Vec3& _pos) const {
        int x = static_cast<int>(_pos.x / cellsize);
        int z = static_cast<int>(_pos.z / cellsize);

        return (static_cast<uint64>(x) << 32) | static_cast<uint64>(z);
    }

    void AddTriangle(int idx, const NavMeshTriangle& _tri) {
        Vec3 minBound = GetMinBound(_tri);
        Vec3 maxBound = GetMaxBound(_tri);

        int minX = static_cast<int>(minBound.x / cellsize);
        int maxX = static_cast<int>(maxBound.x / cellsize);
        int minZ = static_cast<int>(minBound.z / cellsize);
        int maxZ = static_cast<int>(maxBound.z / cellsize);

        for (int x = minX; x <= maxX; x++) {
            for (int z = minZ; z <= maxZ; z++) {
                uint64_t key = (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(z);
                grid[key].emplace_back(idx);
            }
        }
    }

    Vec3 GetMinBound(const NavMeshTriangle& tri) const {
        const Vec3& v0 = tri.vertices[0];
        const Vec3& v1 = tri.vertices[1];
        const Vec3& v2 = tri.vertices[2];
        return Vec3(
            min(min(v0.x, v1.x), v2.x),
            min(min(v0.y, v1.y), v2.y),
            min(min(v0.z, v1.z), v2.z)
        );
    }

    // 주어진 삼각형의 세 정점으로부터 AABB 최대점 반환
    Vec3 GetMaxBound(const NavMeshTriangle& tri) const {
        const Vec3& v0 = tri.vertices[0];
        const Vec3& v1 = tri.vertices[1];
        const Vec3& v2 = tri.vertices[2];
        return Vec3(
            max(max(v0.x, v1.x), v2.x),
            max(max(v0.y, v1.y), v2.y),
            max(max(v0.z, v1.z), v2.z)
        );
    }
};

class NavMesh : public Component
{
    using Super = Component;

public:
    SpatialGrid m_spatialGrid;

    NavMesh();
    virtual ~NavMesh();

    virtual void Start() override;
    virtual void Update() override;

    // 핵심 기능 - 성능 최적화된 참조 버전
    void LoadNavMeshData();
    Vec3 GetNearestPointOnNavMesh(const Vec3& worldPos);
    void FindPath(const Vec3& start, const Vec3& end, vector<Vec3>& outPath); // 참조로 변경
    bool IsOnNavMesh(const Vec3& worldPos, float tolerance = 1.0f);
    bool RaycastNavMesh(const Ray& ray, Vec3& hitPoint);

    // 디버그 기능
    void DebugPrintTriangles();
#if defined(DXA_GAME_PROBE_DIAGNOSTICS)
    size_t GetTriangleCount() const { return m_triangles.size(); }
    size_t GetLastExpandedNodes() const { return m_diagnosticExpanded; }
private:
    size_t m_diagnosticExpanded = 0;
public:
#endif

private:
    // 멤버 변수
    vector<NavMeshTriangle> m_triangles;
    shared_ptr<Model> m_navMeshModel;

    mutable int m_lastFoundTriangle = -1;

    // 초기화 및 전처리
    void BuildTriangleConnections();
    void InitializeSpatialGrid();
    bool AreTrianglesAdjacent(const NavMeshTriangle& tri1, const NavMeshTriangle& tri2);

    // A* 경로찾기 - 성능 최적화된 참조 버전
    bool FindTrianglePath(int startTriangle, int endTriangle, vector<int>& outPath); // 참조로 변경
    void ReconstructPath(const vector<PathNode>& allNodes, int endTriangle, vector<int>& outPath); // 참조로 변경

    // 경로 변환 및 최적화 - 성능 최적화된 참조 버전
    void ConvertTrianglePathToWorldPath(const vector<int>& trianglePath, const Vec3& start, const Vec3& end, vector<Vec3>& outPath); // 참조로 변경
    void SmoothPath(const vector<Vec3>& originalPath, vector<Vec3>& outPath); // 참조로 변경
    void SmoothPathOri(const vector<Vec3>& originalPath, vector<Vec3>& outPath);
    // 유틸리티 함수
    int FindTriangleContaining(const Vec3& point);
    Vec3 ProjectPointOnTriangle(const Vec3& point, const NavMeshTriangle& triangle);
    bool IsPointInTriangle(const Vec3& point, const NavMeshTriangle& triangle);
    bool HasLineOfSight(const Vec3& start, const Vec3& end);
    bool IsLineOnNavMesh(const Vec3& start, const Vec3& end, float stepSize = 0.5f);
    float GetDistance(const Vec3& a, const Vec3& b);
    int GetTotalNeighbors() const;
};