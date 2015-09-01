#include "MaterialData.h"

void MaterialData::Destroy()
{
	for (auto buf : PixelConstantBuffers)
		buf->Release();
}

void CreateStandardMaterial(ID3D11ShaderResourceView* albedoView, const bool isTransparent, MaterialData* materialOut)
{
	materialOut->Type = MATERIAL_TYPE_STANDARD;
	materialOut->IsTransparent = isTransparent;
	materialOut->PixelResourceViews.push_back(albedoView);
}