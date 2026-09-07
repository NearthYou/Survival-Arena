#pragma once

#include "Primitive3D.h"
struct MathUtils
{

	//수학 면접때 이 정도는 대략적으로 설명할 수 있어야 한다. 
	//어떠한 AABB, OBB, Plane에 그 점이 포함되어 있는지 체크하기 위함. 
	//Point Test

	// SphereToPoint
	static bool PointInSphere(const Point3D& _point, const Sphere3D& _sphere);
	static Point3D ClosestPoint(const Sphere3D& _sphere, const Point3D& _point);

	//AABBToPoint
	static bool PointInAABB(const Point3D& _point, const AABB3D& _aabb);
	static Point3D ClosestPoint(const AABB3D& _aabb, const Point3D& _point);

	//OBBToPoint;
	static bool PointInOBB(const Point3D& _point, const OBB3D& _obb);
	static Point3D ClosestPoint(const OBB3D& _obb, const Point3D& _point);

	//Plane
	//어떤 점이 Plane에 있는지 테스트 하고 싶음. 
	//평면을 대상으로 
	//어떤 점이 정확히 평면 위에 있는지 확인 하기 위해서, 어떤 출발점에서, 평먼 중앙으로의 벡터를 구하고.
	//평면의 노말벡터랑 내적을 하면, 따라가는 성분 간의 거리 벡터가 나옴. 
	//PlaneToPoint;
	static bool PointInPlane(const Point3D& _point, const Plane3D& _plane);
	static Point3D ClosestPoint(const Plane3D& _plane, const Point3D& _point);

	//어떤 선분이 있는데 C라는 Point가 그 안에 포함되어 있는가?
	//LineToPoint
	static bool PointInLine(const Point3D& _point, const Line3D& _line);
	static Point3D ClosestPoint(const Line3D& _line, const Point3D& _point);


	//PointAndRay
	//Ray는 원점이 있고 방향이 있을거다. 
	//직선이 있고, P라는 점과 가장 가까운 점을 구하고 싶을 때. 
	//     R
	//     I
	//O -------> R
	static bool PointInRay(const Point3D& _point, const Ray3D& _ray);
	static Point3D ClosestPoint(const Ray3D& _ray, const Point3D& _point);





	//////////////////////////////////////////////////////////
	//Intersections

	static bool SphereSphere(const Sphere3D& _s1, const Sphere3D& _s2);
	static bool SphereAABB(const Sphere3D& _sphere, const AABB3D& _aabb);
	static bool SphereOBB(const Sphere3D& _sphere, const OBB3D& _obb);
	static bool SpherePlane(const Sphere3D& _sphere, const Plane3D& _plane);
	static bool AABBAABB(const AABB3D& _aabb1, const AABB3D& _aabb2);


	//각각 축을 대상으로 테스트 해줘야한다. 
	//어떤 축을 대상으로 투영해서 겹치는 결과가 있으면, 축을 대상으로 통과됨. 
	//투영된 것중에 빈공간이 있다면, return false;
	//SAT방식. 
	static Interval3D GetInterval(const AABB3D& _aabb, const Vec3& _axis);
	static Interval3D GetInterval(const OBB3D& _obb, const Vec3& _axis);
	static bool OverlapOnAxis(const AABB3D& _aabb, const OBB3D& _obb, const Vec3& _axis);
	static bool OverlapOnAxis(const OBB3D& _obb1, const OBB3D& _obb2, const Vec3& _axis);
	static bool AABBOBB(const AABB3D& _aabb, const OBB3D& _obb);
	
	//평면은 무한대로 쭉 빧어나간다. 
	//모든 정점들을 대상으로 위에 있는지 아래에 있는지 판별 
	//내적을 사용하면 된다. 
	static bool AABBPlane(const AABB3D& _aabb, const Plane3D& _plane);
	static bool OBBOBB(const OBB3D& _obb1, const OBB3D& _obb2);
	static bool PlanePlane(const Plane3D& _plane1, const Plane3D& _plane2);

	//////////////////////////////////////////////////////////
	//Raycast

	static bool RayCast(const Sphere3D& _sphere, const Ray3D& _ray, OUT float& _distance);
	static bool RayCast(const AABB3D& _aabb, const Ray3D& _ray, OUT float& _distance);
	static bool RayCast(const Plane3D& _plane, const Ray3D& _ray, OUT float& _distance);

	//학원 포폴 NAV Mesh. 갈 수 있는 범위를 설정하고, 그걸 만들어 주고 싶냐. 
	//그걸 가장 쉽게 하는 방법이 삼각형 여러 개 그려주는 방식. 

	//기본적으로 외적을 사용한다. 방향 벡터를 구한다음에 CROSS로 POINT IN TRIANGLE
	static bool PointInTriangle(const Point3D& _p, const Triangle3D& _t);

	static Plane3D FromTriangle(const Triangle3D& _t);
	static Vec3 Barycentric(const Point3D& _P, const Triangle3D& _t);
	static bool RayCast(const Triangle3D& _triangle, const Ray3D& _ray, OUT float& _distance);
	static Vec3 ProjectVecOnVec(Vec3 _from, Vec3 _to);



	static float Random(float r1, float r2);
	static Vec2 RandomVec2(float r1, float r2);
	static Vec3 RandomVec3(float r1, float r2);
};

