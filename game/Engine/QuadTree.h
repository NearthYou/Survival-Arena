#pragma once
#include "pch.h"
#include <chrono>

class GameObject;
class Camera;
class BaseCollider;

struct QuadTreeNode
{
    RECT bounds;
    vector<shared_ptr<GameObject>> objects;
    unique_ptr<QuadTreeNode> children[4];
    bool isLeaf = true;
    int nodeId = 0; // 디버깅용 노드 ID

    static constexpr int MAX_OBJECTS = 8;
    static constexpr int MAX_DEPTH = 12;

    ~QuadTreeNode();
};

// 성능 통계 구조체
struct QuadTreeStats
{
    size_t lastCollisionPairs = 0;
    int totalNodes = 0;
    int leafNodes = 0;
    int totalObjects = 0;
    int maxDepth = 0;
    float avgObjectsPerLeaf = 0.0f;
    std::chrono::microseconds lastQueryTime{ 0 };
    std::chrono::microseconds lastBuildTime{ 0 };
};
class QuadTree
{
public:
    QuadTree(float _screenWidth, float _screenHeight);
    ~QuadTree();

    void Clear();
    void Insert(shared_ptr<GameObject> _object);
    void Build();
    vector<shared_ptr<GameObject>> Query(const Ray& _ray, shared_ptr<Camera> _camera);


    // 성능 관련
    const QuadTreeStats& GetStats() const { return m_stats; }
    void UpdateStats();

    // 디버그 함수들
    void DebugDraw(shared_ptr<Camera> _camera);
    void PrintTreeStructure();
    void PrintDuplicates();
    void GetNodeBounds(vector<RECT>& _bounds, vector<int>& _depths);

public:
    // 핵심 기능
    void Split(unique_ptr<QuadTreeNode>& _node, int _depth);
    void InsertIntoNode(unique_ptr<QuadTreeNode>& _node, shared_ptr<GameObject> _object, int _depth);
    void QueryNode(const unique_ptr<QuadTreeNode>& _node, const Ray& _ray, shared_ptr<Camera> _camera,
        vector<shared_ptr<GameObject>>& _result);

    // 교차 검사
    bool RayIntersectsAABB(const Ray& _ray, const RECT& _rect, shared_ptr<Camera> _camera);
    bool LineIntersectsRect(const Vec2& _lineStart, const Vec2& _lineEnd, const RECT& _rect);
    bool LineSegmentIntersect(const Vec2& _p1, const Vec2& _q1, const Vec2& _p2, const Vec2& _q2);
    bool PointInRect(const Vec2& _point, const RECT& _rect);


    RECT GetObjectScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera);
    RECT CalculateColliderScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera);
    RECT CalculateColliderAABBScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera);
    RECT CalculateSphereScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera);
    RECT CalculateDefaultScreenBounds(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera);

    // 유틸리티
    bool IsObjectVisible(shared_ptr<GameObject> _object, shared_ptr<Camera> _camera);
    bool IsObjectVisible(shared_ptr<GameObject> _object, Camera* _camera);

    // 디버그 헬퍼
    void DebugDrawNode(const unique_ptr<QuadTreeNode>& _node, int _depth, shared_ptr<Camera> _camera);
    void PrintNodeStructure(const unique_ptr<QuadTreeNode>& _node, int _depth, const string& _prefix);
    void CollectNodeBounds(const unique_ptr<QuadTreeNode>& _node, int _depth, vector<RECT>& _bounds, vector<int>& _depths);
    void CountObjectsInNode(const unique_ptr<QuadTreeNode>& _node, unordered_map<shared_ptr<GameObject>, int>& _objectCount);
    void CalculateStats(const unique_ptr<QuadTreeNode>& _node, int _depth);

    string ws2s(const wstring& _wstr);

    void PrintObjectLocation(shared_ptr<GameObject> _targetObject);
    void PrintObjectInNodes(const unique_ptr<QuadTreeNode>& _node, shared_ptr<GameObject> _targetObject, int _depth);

    Vec2 WorldToScreen(const Vec3& _worldPos, shared_ptr<Camera> _camera);
    Vec3 ScreenToWorld(const Vec2& _screenPos, shared_ptr<Camera> _camera, float _depth = 1.0f);

    void DebugCoordinateTransform(const Vec2& _mousePos, shared_ptr<Camera> _camera);

public:
    unordered_set<shared_ptr<GameObject>>& GetInsertedObject() { return m_insertedObjects; }

//이 밑은 충돌 관련 함수들. 
public:
    void CheckCollisionsInTree(shared_ptr<Camera> _camera, unordered_map<ULONG64, bool>& _collisionMap);
private:
    // 노드별 충돌 검사
    void CheckCollisionsInNode(const unique_ptr<QuadTreeNode>& _node,
        unordered_map<ULONG64, bool>& _collisionMap,
        unordered_set<ULONG64>& _processedPairs);

    // 인접 노드와의 충돌 검사
    void CheckCrossNodeCollisions(const unique_ptr<QuadTreeNode>& _node1,
        const unique_ptr<QuadTreeNode>& _node2,
        unordered_map<ULONG64, bool>& _collisionMap,
        unordered_set<ULONG64>& _processedPairs);

    //인접 노드와의 충돌 검사. 
    void CheckBoundaryCollisions(const unique_ptr<QuadTreeNode>& _node,
        unordered_map<ULONG64, bool>& _collisionMap,
        unordered_set<ULONG64>& _processedPairs);

    //객체와 노드끼리 충돌하는지. 
    void CheckObjectWithNode(shared_ptr<GameObject> _object,
        const unique_ptr<QuadTreeNode>& _node,
        unordered_map<ULONG64, bool>& _collisionMap,
        unordered_set<ULONG64>& _processedPairs);

    //노드끼리 인접성 확인. 
    bool AreNodesAdjacent(const RECT& rect1, const RECT& rect2);

    //충돌 처리 함수. 
    void ProcessCollisionPair(shared_ptr<BaseCollider> _collider1,
        shared_ptr<BaseCollider> _collider2,
        unordered_map<ULONG64, bool>& _collisionMap,
        unordered_set<ULONG64>& _processedPairs);


private:
    unique_ptr<QuadTreeNode> m_root;
    float m_screenWidth;
    float m_screenHeight;

    unordered_set<shared_ptr<GameObject>> m_insertedObjects;

    //디버그용. 
    QuadTreeStats m_stats;
    static int s_nextNodeId;

};

