#ifndef GRAPHICS_DEBUG_H_
#define GRAPHICS_DEBUG_H_

#include <d3d11.h>
#include <string>

inline void SetDebugObjectName(ID3D11DeviceChild* resource, const std::string& str)
{
	if (resource != nullptr)
		resource->SetPrivateData(WKPDID_D3DDebugObjectName, str.length(), str.c_str());
}

#endif