#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <d3d11.h>
#include <vector>
#include <stdint.h>

#define MATERIAL_MAX_RESOURCE_VIEWS 8
#define MATERIAL_MAX_CONSTANT_BUFFERS 8

#define MATERIAL_STANDARD_RESOURCE_COUNT 1
#define MATERIAL_STANDARD_CONSTANT_COUNT 0

enum MaterialType
{
	MATERIAL_TYPE_INVALID,
	MATERIAL_TYPE_STANDARD
};

enum MaterialResourceIndex : uint32_t
{
	MATERIAL_RESOURCE_INDEX_ALBEDO = 0
};

enum MaterialConstantIndex : uint32_t
{
	MATERIAL_CONSTANT_INDEX_LIGHT_DATA = 0
};

// A material class, note that a material owns its constant buffers,
// but does not own it resource views or shaders
class Material
{
public:
	MaterialType Type;

	ID3D11VertexShader* VertexShader;
	ID3D11PixelShader* PixelShader;

	ID3D11ShaderResourceView* PixelResourceViews[MATERIAL_MAX_RESOURCE_VIEWS];
	ID3D11Buffer* PixelConstantBuffers[MATERIAL_MAX_CONSTANT_BUFFERS];

	size_t PixelResourceViewCount;
	size_t PixelConstantBuffersCount;

	bool IsTransparent;

	void Destroy();
};

void CreateStandardMaterial(ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader,
	ID3D11ShaderResourceView* albedoView, const bool isTransparent, Material* materialOut);

#endif