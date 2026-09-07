#pragma once

#include "Geometry.h"
#include "VertexData.h"

class GeometryHelper
{
public:
	//사각형
	static void CreateQuad(shared_ptr<Geometry<VertexColorData>> _geometry, Color _color);

	//큐브 + 구 + 그리드
	static void CreateQuad(shared_ptr<Geometry<VertexTextureData>> geometry);
	static void CreateCube(shared_ptr<Geometry<VertexTextureData>> geometry);
	static void CreateSphere(shared_ptr<Geometry<VertexTextureData>> geometry);
	static void CreateGrid(shared_ptr<Geometry<VertexTextureData>> geometry, int32 sizeX, int32 sizeZ);
	static void CreateCone(shared_ptr<Geometry<VertexTextureData>> geometry, uint32 sliceCount = 20);

	static void CreateQuad(shared_ptr<Geometry<VertexTextureNormalData>> geometry);
	static void CreateCube(shared_ptr<Geometry<VertexTextureNormalData>> geometry);
	static void CreateGrid(shared_ptr<Geometry<VertexTextureNormalData>> geometry, int32 sizeX, int32 sizeZ);
	static void CreateSphere(shared_ptr<Geometry<VertexTextureNormalData>> geometry);
	static void CreateCone(shared_ptr<Geometry<VertexTextureNormalData>> geometry, uint32 sliceCount = 20);


	static void CreateQuad(shared_ptr<Geometry<VertexTextureNormalTangentData>> geometry);
	static void CreateCube(shared_ptr<Geometry<VertexTextureNormalTangentData>> geometry);
	static void CreateGrid(shared_ptr<Geometry<VertexTextureNormalTangentData>> geometry, int32 sizeX, int32 sizeZ);
	static void CreateSphere(shared_ptr<Geometry<VertexTextureNormalTangentData>> geometry);
	static void CreateCone(shared_ptr<Geometry<VertexTextureNormalTangentData>> geometry, uint32 sliceCount = 20);
};

