#ifndef INPUT_ELEMENT_DESC_H_
#define INPUT_ELEMENT_DESC_H_

#include <d3d11.h>

struct InputElementLayout
{
	const D3D11_INPUT_ELEMENT_DESC* Desc;
	size_t AttributeCount;
	size_t Stride;
};

void GetInputElementLayoutStaticMesh(InputElementLayout* layout);
void GetInputElementLayoutStaticMeshInstanced(InputElementLayout* layout);
void GetInputElementLayoutBlit(InputElementLayout* layout);
void GetInputElementLayoutTerrainPatch(InputElementLayout* layout);

#endif