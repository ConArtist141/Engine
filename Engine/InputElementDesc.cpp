#include "InputElementDesc.h"

#define STATIC_MESH_ATTRIBUTE_COUNT 3
#define STATIC_MESH_INSTANCED_ATTRIBUTE_COUNT 7
#define BLIT_ATTRIBUTE_COUNT 2
#define TERRAIN_PATCH_ATTRIBUTE_COUNT 2

#define STATIC_MESH_STRIDE 8 * sizeof(float)
#define BLIT_STRIDE 4 * sizeof(float)
#define TERRAIN_PATCH_STRIDE 6 * sizeof(float)

const D3D11_INPUT_ELEMENT_DESC StaticMeshInputElementDesc[STATIC_MESH_ATTRIBUTE_COUNT] = 
{
	// Data from the vertex buffer
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 5, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

const D3D11_INPUT_ELEMENT_DESC StaticMeshInstancedInputElementDesc[STATIC_MESH_INSTANCED_ATTRIBUTE_COUNT] =
{
	// Data from the vertex buffer
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 5, D3D11_INPUT_PER_VERTEX_DATA, 0 },

	// Data from the instance buffer
	{ "INSTANCE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "INSTANCE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, sizeof(float) * 4, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "INSTANCE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, sizeof(float) * 8, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	{ "INSTANCE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, sizeof(float) * 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
};

const D3D11_INPUT_ELEMENT_DESC BlitInputElementDesc[BLIT_ATTRIBUTE_COUNT] =
{
	// Data from the vertex buffer
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 2, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

const D3D11_INPUT_ELEMENT_DESC TerrainPatchElementDesc[TERRAIN_PATCH_ATTRIBUTE_COUNT] =
{
	// Data from the vertex buffer
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

void GetInputElementLayoutStaticMesh(InputElementLayout * layout)
{
	layout->Desc = StaticMeshInputElementDesc;
	layout->AttributeCount = STATIC_MESH_ATTRIBUTE_COUNT;
	layout->Stride = STATIC_MESH_STRIDE;
}

void GetInputElementLayoutStaticMeshInstanced(InputElementLayout* layout)
{
	layout->Desc = StaticMeshInstancedInputElementDesc;
	layout->AttributeCount = STATIC_MESH_INSTANCED_ATTRIBUTE_COUNT;
	layout->Stride = STATIC_MESH_STRIDE;
}

void GetInputElementLayoutBlit(InputElementLayout* layout)
{
	layout->Desc = BlitInputElementDesc;
	layout->AttributeCount = BLIT_ATTRIBUTE_COUNT;
	layout->Stride = BLIT_STRIDE;
}

void GetInputElementLayoutTerrainPatch(InputElementLayout* layout)
{
	layout->Desc = TerrainPatchElementDesc;
	layout->AttributeCount = TERRAIN_PATCH_ATTRIBUTE_COUNT;
	layout->Stride = TERRAIN_PATCH_STRIDE;
}