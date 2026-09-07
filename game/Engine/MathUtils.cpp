#include "pch.h"
#include "MathUtils.h"

bool MathUtils::PointInSphere(const Point3D& _point, const Sphere3D& _sphere)
{
	float magSq = (_point - _sphere.position).LengthSquared();
	float radSq = _sphere.radius * _sphere.radius;

	return magSq <= radSq;
}

Point3D MathUtils::ClosestPoint(const Sphere3D& _sphere, const Point3D& _point)
{
	Vec3 sphereToPointDir = (_point - _sphere.position);
	sphereToPointDir.Normalize();
	return _sphere.position + sphereToPointDir * _sphere.radius;
}

bool MathUtils::PointInAABB(const Point3D& _point, const AABB3D& _aabb)
{
	Point3D min = AABB3D::GetMin(_aabb);
	Point3D max = AABB3D::GetMax(_aabb);

	if (_point.x < min.x || _point.y < min.y || _point.z < min.z)
		return false;

	if (_point.x > max.x || _point.y > max.y || _point.z > max.z)
		return false;

	return true;
}

Point3D MathUtils::ClosestPoint(const AABB3D& _aabb, const Point3D& _point)
{
	Point3D result = _point;
	Point3D minPt = AABB3D::GetMin(_aabb);
	Point3D maxPt = AABB3D::GetMax(_aabb);

	result.x = max(result.x, minPt.x);
	result.y = max(result.y, minPt.y);
	result.z = max(result.z, minPt.z);

	result.x = min(result.x, maxPt.x);
	result.y = min(result.y, maxPt.y);
	result.z = min(result.z, maxPt.z);

	return result;
}

bool MathUtils::PointInOBB(const Point3D& _point, const OBB3D& _obb)
{
	Vec3 dir = _point - _obb.position;

	vector<Vec3> axis;

	//x,y z축으로 각각 투영해주는데. 
	axis.push_back(_obb.orientation.Right());
	axis.push_back(_obb.orientation.Up());
	axis.push_back(_obb.orientation.Backward());

	vector<float> size;
	size.push_back(_obb.size.x);
	size.push_back(_obb.size.y);
	size.push_back(_obb.size.z);

	for (int32 idx = 0; idx < 3; ++idx) {
		//내적을 통해 해당하는 축의 성분을 본다. 
		float distance = dir.Dot(axis[idx]);

		if (distance > size[idx])
			return false;
		if (distance < -size[idx])
			return false;
	}

	return true;
}

Point3D MathUtils::ClosestPoint(const OBB3D& _obb, const Point3D& _point)
{
	Point3D result = _obb.position;
	Vec3 dir = _point - _obb.position;

	vector<Vec3> axis;
	//x,y z축으로 각각 투영해주는데. 
	axis.push_back(_obb.orientation.Right());
	axis.push_back(_obb.orientation.Up());
	axis.push_back(_obb.orientation.Backward());

	vector<float> size;
	size.push_back(_obb.size.x);
	size.push_back(_obb.size.y);
	size.push_back(_obb.size.z);

	for (int32 idx = 0; idx < 3; ++idx) {
		//내적을 통해 해당하는 축의 성분을 본다. 
		float distance = dir.Dot(axis[idx]);

		if (distance > size[idx])
			distance = size[idx];
		if (distance < -size[idx])
			distance = -size[idx];

		result = result + (axis[idx] * distance);
	}

	return result;
}

//코드를 보고 어떤 의미로 되어 있는지 유추를 할 수 있어야함. 
bool MathUtils::PointInPlane(const Point3D& _point, const Plane3D& _plane)
{
	float dot = _point.Dot(_plane.normal);

	//CMP(dot - plane.distance, 0.0f);
	return dot == _plane.distance;
}

Point3D MathUtils::ClosestPoint(const Plane3D& _plane, const Point3D& _point)
{
	float dot = _point.Dot(_plane.normal);
	float distance = dot - _plane.distance;
	return _point - _plane.normal * distance;
}

bool MathUtils::PointInLine(const Point3D& _point, const Line3D& _line)
{
	Point3D closest = ClosestPoint(_line, _point);
	float distanceSq = (closest - _point).LengthSquared();
	// CMP(distanceSq, 0.0f);
	return distanceSq == 0.f;

}

Point3D MathUtils::ClosestPoint(const Line3D& _line, const Point3D& _point)
{
	Vec3 IVec = _line.end - _line.start; // LINE Vector (방향벡터 구하기)
	//A -> C벡터를 구하고. 그걸 Ivec과 내적. AB와 AC내적. 
	float t = (_point - _line.start).Dot(IVec) / IVec.Dot(IVec);
	//  / IVec.Dot(IVec); 은 뭘까?  크기에다가 AB제곱(크기)로 나눠주기. 

	t = fmaxf(t, 0.0f);
	t = fminf(t, 1.0f);
	return _line.start + IVec * t;


}

bool MathUtils::PointInRay(const Point3D& _point, const Ray3D& _ray)
{
	if (_point == _ray.origin) 
		return true;

	Vec3 norm = _point - _ray.origin;
	norm.Normalize();

	float diff = norm.Dot(_ray.direction);
	return diff == 1.0f;
}

Point3D MathUtils::ClosestPoint(const Ray3D& _ray, const Point3D& _point)
{
	float t = (_point - _ray.origin).Dot(_ray.direction);

	t = fmaxf(t, 0.0f);
	return Point3D(_ray.origin + _ray.direction * t);
}

bool MathUtils::SphereSphere(const Sphere3D& _s1, const Sphere3D& _s2)
{
	float sum = _s1.radius + _s2.radius;
	float sqDistance = (_s1.position - _s2.position).LengthSquared();
	return sqDistance <= sum * sum;
}

bool MathUtils::SphereAABB(const Sphere3D& _sphere, const AABB3D& _aabb)
{
	Point3D closestPoint = ClosestPoint(_aabb, _sphere.position);
	float distSq = (_sphere.position - closestPoint).LengthSquared();
	float radiusSq = _sphere.radius * _sphere.radius;
	return distSq < radiusSq;
}

bool MathUtils::SphereOBB(const Sphere3D& _sphere, const OBB3D& _obb)
{
	Point3D closestPoint = ClosestPoint(_obb, _sphere.position);
	float distSq = (_sphere.position - closestPoint).LengthSquared();
	float radiusSq = _sphere.radius * _sphere.radius;
	return distSq < radiusSq;
}

bool MathUtils::SpherePlane(const Sphere3D& _sphere, const Plane3D& _plane)
{
	Point3D closestPoint = ClosestPoint(_plane, _sphere.position);
	float distSq = (_sphere.position - closestPoint).LengthSquared();
	float radiusSq = _sphere.radius * _sphere.radius;
	return distSq < radiusSq;
}

bool MathUtils::AABBAABB(const AABB3D& _aabb1, const AABB3D& _aabb2)
{
	Point3D aMin = AABB3D::GetMin(_aabb1);
	Point3D aMax = AABB3D::GetMax(_aabb1);
	Point3D bMin = AABB3D::GetMin(_aabb2);
	Point3D bMax = AABB3D::GetMax(_aabb2);

	return (aMin.x <= bMax.x && aMax.x >= bMin.x) &&
		(aMin.y <= bMax.y && aMax.y >= bMin.y) &&
		(aMin.z <= bMax.z && aMax.z >= bMin.z);
}

Interval3D MathUtils::GetInterval(const AABB3D& _aabb, const Vec3& _axis)
{
	//AABB가 정점이 8개가 있을 것. 사각형이니까.
	//일단 그 8개의 정점을 싹 다 만들고 시작. 
	Vec3 i = AABB3D::GetMin(_aabb);
	Vec3 a = AABB3D::GetMax(_aabb);
	
	Vec3 vertex[8] = {
		Vec3(i.x, a.y, a.z),
		Vec3(i.x, a.y, i.z),
		Vec3(i.x, i.y, a.z),
		Vec3(i.x, i.y, i.z),
		Vec3(a.x, a.y, a.z),
		Vec3(a.x, a.y, i.z),
		Vec3(a.x, i.y, a.z),
		Vec3(a.x, i.y, i.z),
	};

	// 최소 / 최대 구하기
	Interval3D result;
	result.min = result.max = _axis.Dot(vertex[0]);

	//min max계속 적용하면서, projection해서 가장 큰 값, 작은 값 구해서 리턴한다. 
	for (int idx = 1; idx < 8; ++idx) {
		float projection = _axis.Dot(vertex[idx]);

		result.min = min(result.min, projection);
		result.max = max(result.max, projection);
	}

	return result;
}

Interval3D MathUtils::GetInterval(const OBB3D& _obb, const Vec3& _axis)
{
	Vec3 vertex[8];

	Vec3 C = _obb.position; //OBB Center;
	Vec3 E = _obb.size;

	vector<Vec3> A; // OBB Axis
	A.push_back(_obb.orientation.Right());
	A.push_back(_obb.orientation.Up());
	A.push_back(_obb.orientation.Backward());

	//8개의 좌표를 구해줌. 회전 적용해서 복잡함. 
	vertex[0] = C + A[0] * E.x + A[1] * E.y + A[2] * E.z;
	vertex[1] = C - A[0] * E.x + A[1] * E.y + A[2] * E.z;
	vertex[2] = C + A[0] * E.x - A[1] * E.y + A[2] * E.z;
	vertex[3] = C + A[0] * E.x + A[1] * E.y - A[2] * E.z;
	vertex[4] = C - A[0] * E.x - A[1] * E.y - A[2] * E.z;
	vertex[5] = C + A[0] * E.x - A[1] * E.y - A[2] * E.z;
	vertex[6] = C - A[0] * E.x + A[1] * E.y - A[2] * E.z;
	vertex[7] = C - A[0] * E.x - A[1] * E.y + A[2] * E.z;

	// 최소 / 최대 구하기
	Interval3D result;
	result.min = result.max = _axis.Dot(vertex[0]);

	//min max계속 적용하면서, projection해서 가장 큰 값, 작은 값 구해서 리턴한다. 
	for (int idx = 1; idx < 8; ++idx) {
		float projection = _axis.Dot(vertex[idx]);

		result.min = min(result.min, projection);
		result.max = max(result.max, projection);
	}

	return result;
}

bool MathUtils::OverlapOnAxis(const AABB3D& _aabb, const OBB3D& _obb, const Vec3& _axis)
{
	Interval3D a = GetInterval(_aabb, _axis);
	Interval3D b = GetInterval(_obb, _axis);

	return ((b.min <= a.max) && (a.min <= b.max));
}

bool MathUtils::OverlapOnAxis(const OBB3D& _obb1, const OBB3D& _obb2, const Vec3& _axis)
{
	Interval3D a = GetInterval(_obb1, _axis);
	Interval3D b = GetInterval(_obb2, _axis);

	return ((b.min <= a.max) && (a.min <= b.max));
}

bool MathUtils::AABBOBB(const AABB3D& _aabb, const OBB3D& _obb)
{
	Vec3 test[15] =
	{
		Vec3(1, 0, 0),// AABB axis 1,2,3
		Vec3(0, 1, 0),
		Vec3(0, 0, 1),
		_obb.orientation.Right(), //OBB axis 1,2,3
		_obb.orientation.Up(),
		_obb.orientation.Backward(),
	};

	//외적을 통해 구한 축. 
	//예외 없이 모든 테스트가 끝난다. 
	for (int idx = 0; idx < 3; ++idx) {
		test[6 + idx * 3 + 0] = test[idx].Cross(test[0]);
		test[6 + idx * 3 + 1] = test[idx].Cross(test[1]);
		test[6 + idx * 3 + 2] = test[idx].Cross(test[2]);
	}

	for (int idx = 0; idx < 15; ++idx) {
		if (OverlapOnAxis(_aabb, _obb, test[idx]) == false)
			return false; //하나라도 빈틈이 있다면. 
	}
	//빈틈이 없다면 true
	return true;
}


//분석해보기.... 
bool MathUtils::AABBPlane(const AABB3D& _aabb, const Plane3D& _plane)
{
	float pLen = _aabb.size.x * fabsf(_plane.normal.x) +
		_aabb.size.y * fabsf(_plane.normal.y) +
		_aabb.size.z * fabsf(_plane.normal.z);

	float dot = _plane.normal.Dot(_aabb.position);
	float dist = dot - _plane.distance;

	return fabsf(dist) <= pLen;
}

bool MathUtils::OBBOBB(const OBB3D& _obb1, const OBB3D& _obb2)
{
	Vec3 test[15] =
	{
		_obb1.orientation.Right(), //OBB axis 1,2,3
		_obb1.orientation.Up(),
		_obb1.orientation.Backward(),
		_obb2.orientation.Right(), //OBB axis 1,2,3
		_obb2.orientation.Up(),
		_obb2.orientation.Backward(),
	};

	//외적을 통해 구한 축. 
	//예외 없이 모든 테스트가 끝난다. 
	for (int idx = 0; idx < 3; ++idx) {
		test[6 + idx * 3 + 0] = test[idx].Cross(test[0]);
		test[6 + idx * 3 + 1] = test[idx].Cross(test[1]);
		test[6 + idx * 3 + 2] = test[idx].Cross(test[2]);
	}

	for (int idx = 0; idx < 15; ++idx) {
		if (OverlapOnAxis(_obb1, _obb2, test[idx]) == false)
			return false; //하나라도 빈틈이 있다면. 
	}
	//빈틈이 없다면 true
	return true;
}

bool MathUtils::PlanePlane(const Plane3D& _plane1, const Plane3D& _plane2)
{
	Vec3 d = _plane1.normal.Cross(_plane2.normal);
	//평행하지 않으면 무조건 겹치게 됨. 
	return d.Dot(d) != 0;
}

bool MathUtils::RayCast(const Sphere3D& _sphere, const Ray3D& _ray, OUT float& _distance)
{
	Vec3 e = _sphere.position - _ray.origin;

	float rSq = _sphere.radius * _sphere.radius;
	float eSq = e.LengthSquared();

	float a = e.Dot(_ray.direction);

	float bSq = eSq - (a * a);
	float f = sqrt(rSq - bSq);

	//No Collision has happened
	
	if (rSq - (eSq - (a * a)) < 0.0f)
		return false;

	//Ray starts inside the sphere
	if (eSq < rSq) {
		_distance = a + f;
		return true;
	}

	_distance = a - f;
	return true;
}

bool MathUtils::RayCast(const AABB3D& _aabb, const Ray3D& _ray, OUT float& _distance)
{
	Vec3 min = AABB3D::GetMin(_aabb);
	Vec3 max = AABB3D::GetMax(_aabb);


	//TODO : 0 나누기 방지 위해 + @ 더해줌

	float t1 = (min.x - _ray.origin.x) / _ray.direction.x;
	float t2 = (max.x - _ray.origin.x) / _ray.direction.x;

	float t3 = (min.y - _ray.origin.y) / _ray.direction.y;
	float t4 = (max.y - _ray.origin.y) / _ray.direction.y;

	float t5 = (min.z - _ray.origin.z) / _ray.direction.z;
	float t6 = (max.z - _ray.origin.z) / _ray.direction.z;

	float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
	float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

	if (tmax < 0)
		return false;
	if (tmin > tmax)
		return false;
	if (tmin < 0.0f) {
		_distance = tmax;
		return true;
	}

	_distance = tmin;
	return true;
}

bool MathUtils::RayCast(const Plane3D& _plane, const Ray3D& _ray, OUT float& _distance)
{
	float nd = _ray.direction.Dot(_plane.normal);
	float pn = _ray.direction.Dot(_plane.normal);

	if (nd >= 0.0f)
		return false;

	float t = (_plane.distance - pn) / nd;
	if (t >= 0.0f) {
		_distance = t;
		return true;
	}
	return false;
}

bool MathUtils::PointInTriangle(const Point3D& _p, const Triangle3D& _t)
{
	Vec3 a = _t.a;
	Vec3 b = _t.b;
	Vec3 c = _t.c;

	Vec3 normPBC = b.Cross(c);	//Normal of PBC (u)
	Vec3 normPCA = c.Cross(a);	//Normal of PCA (v)
	Vec3 normPAB = a.Cross(b);	//Normal of PAB (w)


	//이거 반대 방향이면 return.

	//전형적인 외적의 사용 예제. 
	if (normPBC.Dot(normPCA) < 0.0f)
		return false;
	else if (normPBC.Dot(normPAB) < 0.0f)
		return false;
	return true;
}
//삼각형이 있을 때, 삼각형을 이용해서 Plane을 만드는 유틸.
Plane3D MathUtils::FromTriangle(const Triangle3D& _t)
{
	Plane3D result;
	result.normal = (_t.b - _t.a).Cross(_t.c - _t.a);
	result.normal.Normalize();

	result.distance = result.normal.Dot(_t.a);

	return result;
}

Vec3 MathUtils::Barycentric(const Point3D& _p, const Triangle3D& _t)
{
	
	Vec3 ap = _p - _t.a;
	Vec3 bp = _p - _t.b;
	Vec3 cp = _p - _t.c;

	Vec3 ab = _t.b - _t.a;
	Vec3 ac = _t.c - _t.a;
	Vec3 bc = _t.c - _t.b;
	Vec3 cb = _t.b - _t.c;
	Vec3 ca = _t.a - _t.c;

	Vec3 v = ab - ProjectVecOnVec(ab, cb);
	float a = 1.0f - (v.Dot(ap) / v.Dot(ab));

	v = bc - ProjectVecOnVec(bc, ac);
	float b = 1.0f - (v.Dot(bp) / v.Dot(bc));

	v = ca - ProjectVecOnVec(ca, ab);
	float c = 1.0f - (v.Dot(cp) / v.Dot(ca));

	return Vec3(a, b, c);
}




//삼각형을 평면으로 만들어서, 피격하는 정점 찾아서
//그 정점이 삼각형 안에 있는지. 
bool MathUtils::RayCast(const Triangle3D& _triangle, const Ray3D& _ray, OUT float& _distance)
{
	Plane3D plane = FromTriangle(_triangle);

	float t = 0;
	if (RayCast(plane, _ray, OUT t) == false)
		return false;


	Point3D result = _ray.origin + _ray.direction * t;
	Vec3 barycentric = Barycentric(result, _triangle);

	if (barycentric.x >= 0.0f && barycentric.x <= 1.0f &&
		barycentric.y >= 0.0f && barycentric.y <= 1.0f &&
		barycentric.z >= 0.0f && barycentric.z <= 1.0f) {
		_distance = t;
		return true;
	}

	return false;
}

Vec3 MathUtils::ProjectVecOnVec(Vec3 _from, Vec3 _to)
{
	//벡터에서 투영해서 다른 벡터의 선분을 구하는 것. 
	_to.Normalize();
	float dist = _from.Dot(_to);

	return _to * dist;
}

float MathUtils::Random(float r1, float r2)
{
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = r2 - r1;
	float val = random * diff;

	return r1 + val;
}

Vec2 MathUtils::RandomVec2(float r1, float r2)
{
	Vec2 result;
	result.x = Random(r1, r2);
	result.y = Random(r1, r2);

	return result;
}

Vec3 MathUtils::RandomVec3(float r1, float r2)
{
	Vec3 result;
	result.x = Random(r1, r2);
	result.y = Random(r1, r2);
	result.z = Random(r1, r2);

	return result;
}


