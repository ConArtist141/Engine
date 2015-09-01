#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <d3d11.h>
#include <vector>
#include <stdint.h>

enum MaterialType
{
	MATERIAL_TYPE_INVALID,
	MATERIAL_TYPE_STANDARD
};

enum MaterialResourceIndex
{
	MATERIAL_RESOURCE_INDEX_ALBEDO = 0
};

enum MaterialConstantIndex
{
	MATERIAL_CONSTANT_INDEX_LIGHT_DATA = 0
};

// A material class, note that a material owns its constant buffers,
// but does not own it resource views or shaders
class MaterialData
{
public:
	MaterialType Type;

	std::vector<ID3D11ShaderResourceView*> PixelResourceViews;
	std::vector<ID3D11Buffer*> PixelConstantBuffers;

	bool IsTransparent;

	void Destroy();
};

void CreateStandardMaterial(ID3D11ShaderResourceView* albedoView, const bool isTransparent, MaterialData* materialOut);

#endif