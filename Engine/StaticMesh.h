#ifndef STATIC_MESH_H_
#define STATIC_MESH_H_

#include <d3d11.h>
#include <stdio.h>
#include <map>
#include <string>

#include "Geometry.h"

class StaticMesh
{
protected:
	ID3D11Buffer* vertexBuffer;
	ID3D11Buffer* indexBuffer;

	size_t indexCount;
	size_t indexOffset;

	Bounds meshBounds;
	DXGI_FORMAT indexFormat;

public:
	StaticMesh(ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, 
		size_t indexCount, size_t indexOffset, const Bounds& bounds,
		const DXGI_FORMAT indexFormat);

	inline ID3D11Buffer* GetVertexBuffer() const;
	inline ID3D11Buffer* GetIndexBuffer() const;
	inline DXGI_FORMAT GetIndexFormat() const;
	inline size_t GetIndexCount() const;
	inline size_t GetIndexOffset() const;
	inline void GetMeshBounds(Bounds* boundsOut) const;

	void Destroy();
};

inline ID3D11Buffer* StaticMesh::GetVertexBuffer() const		{ return vertexBuffer; }
inline ID3D11Buffer* StaticMesh::GetIndexBuffer() const			{ return indexBuffer; }
inline DXGI_FORMAT StaticMesh::GetIndexFormat() const			{ return indexFormat; }
inline size_t StaticMesh::GetIndexCount() const					{ return indexCount; }
inline size_t StaticMesh::GetIndexOffset()	const				{ return indexOffset; }
inline void StaticMesh::GetMeshBounds(Bounds* boundsOut) const	{ *boundsOut = meshBounds; }

#define VERTEX_ATTRIBUTE_DISABLED -1

#endif