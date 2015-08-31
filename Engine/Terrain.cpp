#include "Terrain.h"

#include <limits>
#include <vector>

using namespace DirectX;
using namespace std;

void TerrainPatch::DestroyMesh()
{
	if (MeshData.VertexBuffer != nullptr)
		MeshData.VertexBuffer->Release();
	if (MeshData.IndexBuffer != nullptr)
		MeshData.IndexBuffer->Release();
	ZeroMemory(&MeshData, sizeof(MeshData));
}

TerrainPatchError TerrainPatch::GenerateVertexBuffer(const size_t mipLevel, ID3D11Device* device)
{
	assert(MeshData.VertexBuffer == nullptr);

	// Position and normal
	size_t elementByteWidth = 2 * sizeof(XMFLOAT3);
	size_t elementCount = MipLevels[mipLevel].ExtentX * MipLevels[mipLevel].ExtentY;
	unique_ptr<XMFLOAT3[]> vertexData(new XMFLOAT3[elementCount]);

	XMVECTOR offsetVec = XMLoadFloat3(&MeshOffset);

	// Compute positions
	size_t extentX = MipLevels[mipLevel].ExtentX;
	size_t extentY = MipLevels[mipLevel].ExtentY;

	for (size_t yLoc = 0, i = 0; yLoc < extentY; ++yLoc)
	{
		for (size_t xLoc = 0; xLoc < extentX; ++xLoc, ++i)
		{
			XMVECTOR posVec = XMVectorSet(static_cast<float>(xLoc) * CellSize.x, 
				MipLevels[mipLevel].Heights[i] * CellSize.y, 
				static_cast<float>(yLoc) * CellSize.z, 
				1.0f);
			posVec += offsetVec;

			XMStoreFloat3(&vertexData[2 * i], posVec);
		}
	}

	// Compute normals
	XMVECTOR zeroVec = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	for (size_t yLoc = 0, i = 0; yLoc < extentY; ++yLoc)
	{
		for (size_t xLoc = 0; xLoc < extentX; ++xLoc, ++i)
		{
			XMVECTOR currentPos = XMLoadFloat3(&vertexData[2 * i]);
			XMVECTOR xDirPos = zeroVec;
			XMVECTOR xDirNeg = zeroVec;
			XMVECTOR zDirPos = zeroVec;
			XMVECTOR zDirNeg = zeroVec;
			XMVECTOR normalVec = zeroVec;

			if (xLoc < extentX - 1)
				xDirPos = XMLoadFloat3(&vertexData[2 * (i + 1)]) - currentPos;
			if (yLoc < extentY - 1)
				zDirPos = XMLoadFloat3(&vertexData[2 * (i + extentX)]) - currentPos;
			if (xLoc > 0)
				xDirNeg = XMLoadFloat3(&vertexData[2 * (i - 1)]) - currentPos;
			if (yLoc > 0)
				zDirNeg = XMLoadFloat3(&vertexData[2 * (i - extentX)]) - currentPos;

			normalVec += XMVector3Cross(zDirPos, xDirPos);
			normalVec += XMVector3Cross(xDirPos, zDirNeg);
			normalVec += XMVector3Cross(zDirNeg, xDirNeg);
			normalVec += XMVector3Cross(xDirNeg, zDirPos);

			normalVec = XMVector3Normalize(normalVec);

			XMStoreFloat3(&vertexData[2 * i + 1], normalVec);
		}
	}
	
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.ByteWidth = elementByteWidth * elementCount;
	desc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA bufData;
	ZeroMemory(&bufData, sizeof(bufData));
	bufData.pSysMem = vertexData.get();

	HRESULT result = device->CreateBuffer(&desc, &bufData, &MeshData.VertexBuffer);

	if (FAILED(result))
		return TERRAIN_PATCH_ERROR_VERTEX_BUFFER_CREATION_FAILED;

	return TERRAIN_PATCH_ERROR_OK;
}

TerrainPatchError TerrainPatch::GenerateIndexBuffer(const size_t mipLevel, ID3D11Device* device)
{
	assert(MeshData.IndexBuffer == nullptr);

	vector<uint16_t> indices;

	size_t extentX = MipLevels[mipLevel].ExtentX;
	size_t extentY = MipLevels[mipLevel].ExtentY - 1;

	size_t yParity;
	size_t yParityBegin = 0;
	for (size_t yLoc = 0; yLoc < extentY; ++yLoc)
	{
		size_t xLoc = 0;

		yParity = yParityBegin;
		indices.push_back(static_cast<uint16_t>(xLoc + (yLoc + yParity) * extentX));

		for (; xLoc < extentX; ++xLoc)
		{
			indices.push_back(static_cast<uint16_t>(xLoc + (yLoc + yParity) * extentX));
			yParity = (yParity + 1) % 2;
			indices.push_back(static_cast<uint16_t>(xLoc + (yLoc + yParity) * extentX));
		}
		--xLoc;

		indices.push_back(static_cast<uint16_t>(xLoc + (yLoc + yParity) * extentX));
		yParityBegin = (yParityBegin + 1) % 2;
	}

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.ByteWidth = sizeof(uint16_t) * indices.size();
	desc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA bufData;
	ZeroMemory(&bufData, sizeof(bufData));
	bufData.pSysMem = indices.data();

	HRESULT result = device->CreateBuffer(&desc, &bufData, &MeshData.IndexBuffer);

	if (FAILED(result))
		return TERRAIN_PATCH_ERROR_INDEX_BUFFER_CREATION_FAILED;

	return TERRAIN_PATCH_ERROR_OK;
}

TerrainPatchError TerrainPatch::GenerateMesh(const size_t mipLevel, ID3D11Device* device)
{
	TerrainPatchError result;
	CurrentMip = mipLevel;
	DestroyMesh();

	result = GenerateVertexBuffer(mipLevel, device);
	if (!result)
		return result;

	result = GenerateIndexBuffer(mipLevel, device);
	if (!result)
		return result;

	return TERRAIN_PATCH_ERROR_OK;
}

void HeightField::ComputeHeightBounds()
{
	size_t count = ExtentX * ExtentY;
	HeightBounds.Min = std::numeric_limits<float>::infinity();
	HeightBounds.Max = -HeightBounds.Min;
	for (size_t i = 0; i < count; ++i)
	{
		auto height = Heights[0];
		if (height < HeightBounds.Min)
			HeightBounds.Min = height;
		if (height > HeightBounds.Max)
			HeightBounds.Max = height;
	}
}