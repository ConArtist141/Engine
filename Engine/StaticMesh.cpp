#include "StaticMesh.h"

StaticMesh::StaticMesh(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer,
	size_t indexCount, size_t indexOffset, const Bounds& bounds,
	const DXGI_FORMAT indexFormat) :
	vertexBuffer(vertexBuffer),
	indexBuffer(indexBuffer),
	indexCount(indexCount),
	indexOffset(indexOffset),
	meshBounds(bounds),
	indexFormat(indexFormat)
{
}

void StaticMesh::Destroy()
{
	vertexBuffer->Release();
	indexBuffer->Release();
}