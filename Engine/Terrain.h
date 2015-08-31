#ifndef TERRAIN_H_
#define TERRAIN_H_

#include <memory>
#include <d3d11.h>
#include <DirectXMath.h>

#include "Geometry.h"

enum TerrainPatchError
{
	TERRAIN_PATCH_ERROR_OK = 0,
	TERRAIN_PATCH_ERROR_VERTEX_BUFFER_CREATION_FAILED = 1,
	TERRAIN_PATCH_ERROR_INDEX_BUFFER_CREATION_FAILED = 2
};

const D3D11_INPUT_ELEMENT_DESC* GetInputElementDescTerrainPatch();

class HeightField
{
public:
	std::unique_ptr<float[]> Heights;
	size_t ExtentX;
	size_t ExtentY;

	struct
	{
		float Max;
		float Min;
	} HeightBounds;

	void ComputeHeightBounds();

	inline float operator()(const int x, const int y) const;
};

class TerrainPatch
{
public:
	std::unique_ptr<HeightField[]> MipLevels;
	size_t MipCount;

	struct
	{
		ID3D11Buffer* VertexBuffer;
		ID3D11Buffer* IndexBuffer;
		size_t VertexCount;
		size_t IndexCount;
	} MeshData;

	size_t CurrentMip;
	DirectX::XMFLOAT3 CellSize;
	DirectX::XMFLOAT3 MeshOffset;

	void DestroyMesh();
	TerrainPatchError GenerateVertexBuffer(const size_t mipLevel, ID3D11Device * device);
	TerrainPatchError GenerateIndexBuffer(const size_t mipLevel, ID3D11Device * device);
	TerrainPatchError GenerateMesh(const size_t mipLevel, ID3D11Device * device);

	inline size_t GetPatchExtentX() const;
	inline size_t GetPatchExtentY() const;
	inline void GetBounds(Bounds* boundsOut) const;
};

inline size_t TerrainPatch::GetPatchExtentX() const
{
	return MipLevels[0].ExtentX;
}

inline size_t TerrainPatch::GetPatchExtentY() const
{
	return MipLevels[0].ExtentY;
}

inline void TerrainPatch::GetBounds(Bounds* boundsOut) const
{
	boundsOut->Lower = MeshOffset;
	boundsOut->Lower.y += CellSize.y * MipLevels[0].HeightBounds.Min;
	boundsOut->Upper = boundsOut->Lower;
	boundsOut->Upper.x += CellSize.x * static_cast<float>(MipLevels[0].ExtentX);
	boundsOut->Upper.y += CellSize.y * MipLevels[0].HeightBounds.Max;
	boundsOut->Upper.z += CellSize.z * static_cast<float>(MipLevels[0].ExtentY);
}

inline float HeightField::operator()(const int x, const int y) const
{
	return Heights[y * ExtentX + x];
}

#endif