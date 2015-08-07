#include "Material.h"

void Material::Destroy()
{
	for (uint32_t i = 0; i < PixelConstantBuffersCount; ++i)
		PixelConstantBuffers[i]->Release();
}

void CreateStandardMaterial(ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader,
	ID3D11ShaderResourceView* albedoView, const bool isTransparent, Material* materialOut)
{
	materialOut->Type = MATERIAL_TYPE_STANDARD;
	materialOut->IsTransparent = isTransparent;
	materialOut->PixelResourceViews[MATERIAL_RESOURCE_INDEX_ALBEDO] = albedoView;
	materialOut->PixelShader = pixelShader;
	materialOut->VertexShader = vertexShader;
	materialOut->PixelResourceViewCount = MATERIAL_STANDARD_RESOURCE_COUNT;
	materialOut->PixelConstantBuffersCount = MATERIAL_STANDARD_CONSTANT_COUNT;
}