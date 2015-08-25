#include "Renderer.h"
#include "Camera.h"
#include "SceneGraph.h"
#include "ContentPackage.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;
using namespace DirectX;

#define SAFE_RELEASE(x) if (x != nullptr) x->Release();

const D3D11_INPUT_ELEMENT_DESC StaticMeshInputElementDesc[STATIC_MESH_VERTEX_ATTRIBUTE_COUNT] =
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
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 2, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

template <typename CacheData>
inline size_t ResizingCache<CacheData>::GetSize() const				{ return cacheSize; }
template <typename CacheData>
inline size_t ResizingCache<CacheData>::GetReservedSize() const		{ return cacheReserved; }
template <typename CacheData>
inline CacheData* ResizingCache<CacheData>::GetData() const			{ return cache; }
template <typename CacheData>
ResizingCache<CacheData>::ResizingCache(const size_t reserve) :
cache(new CacheData[reserve]), cacheReserved(reserve), cacheSize(0)	{ }

template <typename CacheData>
void ResizingCache<CacheData>::Reserve(const size_t reserveCount)
{
	if (reserveCount < cacheReserved)
	{
		delete[] cache;
		cacheReserved = reserveCount;
		cache = new CacheData[cacheReserved];
	}
	else if (reserveCount > cacheReserved)
	{
		auto newCache = new CacheData[reserveCount];
		memcpy(newCache, cache, sizeof(CacheData) * cacheReserved);
		delete[] cache;
		cacheReserved = reserveCount;
		cache = newCache;
	}
}

template <typename CacheData>
inline void ResizingCache<CacheData>::Push(const CacheData& data)
{
	if (cacheSize == cacheReserved)
		Reserve(cacheSize * 2);

	cache[cacheSize] = data;
	++cacheSize;
}

template <typename CacheData>
inline void ResizingCache<CacheData>::Clear()
{
	cacheSize = 0;
}

Renderer::Renderer() :
bMoveSizeEntered(false),
bDisposed(false),
swapChain(nullptr),
device(nullptr),
deviceContext(nullptr),
backBufferRenderTarget(nullptr),
defaultDepthStencilView(nullptr),
depthStencilTexture(nullptr),
forwardPassDepthStencilState(nullptr),
blitDepthStencilState(nullptr),
forwardPassRasterState(nullptr),
linearSamplerState(nullptr),
staticMeshInputLayout(nullptr),
blitPixelShader(nullptr),
blitVertexShader(nullptr),
blitInputLayout(nullptr),
blitVertexBuffer(nullptr),
blitSamplerState(nullptr),
internalContent(nullptr),
frameCount(0),
instanceCache(DEFAULT_INSTANCE_CACHE_SIZE)
{
}

const D3D11_INPUT_ELEMENT_DESC* Renderer::GetStaticMeshInputElementDesc() const
{
	return StaticMeshInputElementDesc;
}

const D3D11_INPUT_ELEMENT_DESC* Renderer::GetBlitInputElementDesc() const
{
	return BlitInputElementDesc;
}

bool Renderer::Initialize(HWND hWindow, const RenderParams& params)
{
	HRESULT result;
	result = InitWindow(hWindow, params);
	if (FAILED(result))
		return false;

	result = InitRenderTarget();
	if (FAILED(result))
		return false;

	result = InitRenderObjects();
	if (FAILED(result))
		return false;

	internalContent = new ContentPackage(this);

	result = InitInternalShaders();
	if (FAILED(result))
		return false;

	result = InitInternalVertexBuffers();
	if (FAILED(result))
		return false;

	return true;
}

bool Renderer::InitWindow(const HWND hWindow, const RenderParams& params)
{
	renderParameters = params;

	IDXGIFactory* factory;
	HRESULT result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);

	IDXGIAdapter* adapter;
	result = factory->EnumAdapters(0, &adapter);

	IDXGIOutput* adapterOutput;
	result = adapter->EnumOutputs(0, &adapterOutput);

	UINT modeCount;
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_B8G8R8A8_UNORM, 
		DXGI_ENUM_MODES_INTERLACED, &modeCount, nullptr);

	DXGI_MODE_DESC* modeDescriptions = new DXGI_MODE_DESC[modeCount];
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, 
		DXGI_ENUM_MODES_INTERLACED, &modeCount, modeDescriptions);

	DXGI_MODE_DESC* descMatch = nullptr;

	for (UINT i = 0; i < modeCount; ++i)
	{
		DXGI_MODE_DESC* desc = &modeDescriptions[i];

		if (desc->Width == params.Extent.Width && desc->Height == params.Extent.Height)
		{
			OutputDebugString("Found compatible display mode!\n");
			descMatch = desc;
			break;
		}
	}

	if (descMatch == nullptr)
	{
		OutputDebugString("No DXGI mode match found - using a default!\n");
		descMatch = modeDescriptions;
	}

	DXGI_ADAPTER_DESC adapterDesc;
	result = adapter->GetDesc(&adapterDesc);

	adapterOutput->Release();
	adapter->Release();
	factory->Release();

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));

	swapChainDesc.Windowed = params.Windowed;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc = *descMatch;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = hWindow;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	delete[] modeDescriptions;

	UINT deviceCreationFlags = 0;
#ifdef ENABLE_DIRECT3D_DEBUG
	deviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	if (!renderParameters.Windowed)
		swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	OutputDebugString("Creating device and swap chain...\n");

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		deviceCreationFlags, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc,
		&swapChain, &device, nullptr, &deviceContext);

	if (FAILED(result))
	{
		OutputDebugString("Failed to create device and swap chain!\n");
		return false;
	}

	OutputDebugString("Device and swap chain created successfully!\n");

	return true;
}

bool Renderer::InitRenderTarget()
{
	OutputDebugString("Creating render target view for back buffer...\n");

	ID3D11Texture2D* backBuffer;
	HRESULT result = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	result = device->CreateRenderTargetView(backBuffer, nullptr, &backBufferRenderTarget);
	result = backBuffer->Release();

	OutputDebugString("Creating depth texture...\n");

	D3D11_TEXTURE2D_DESC depthTextureDesc;
	ZeroMemory(&depthTextureDesc, sizeof(depthTextureDesc));

	depthTextureDesc.Width = renderParameters.Extent.Width;
	depthTextureDesc.Height = renderParameters.Extent.Height;
	depthTextureDesc.MipLevels = 1;
	depthTextureDesc.ArraySize = 1;
	depthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthTextureDesc.SampleDesc.Count = 1;
	depthTextureDesc.SampleDesc.Quality = 0;
	depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthTextureDesc.CPUAccessFlags = 0;
	depthTextureDesc.MiscFlags = 0;

	result = device->CreateTexture2D(&depthTextureDesc, nullptr, &depthStencilTexture);

	if (FAILED(result))
		return false;

	if (FAILED(result))
		return false;

	OutputDebugString("Creating depth stencil view...\n");

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	result = device->CreateDepthStencilView(depthStencilTexture, &depthStencilViewDesc, &defaultDepthStencilView);

	if (FAILED(result))
		return false;

	return true;
}

bool Renderer::CreateStaticMeshInputLayout(const std::string& vertexShaderFile)
{
	char* byteCode = nullptr;
	size_t byteCodeLength;
	std::ifstream t(vertexShaderFile, std::ios::binary | std::ios::in);
	if (t.fail())
		return false;

	t.seekg(0, std::ios::end);
	byteCodeLength = static_cast<size_t>(t.tellg());
	t.seekg(0, std::ios::beg);

	byteCode = new char[byteCodeLength];
	t.read(byteCode, byteCodeLength);
	t.close();

	BytecodeBlob blob = { byteCode, byteCodeLength };
	bool result = CreateStaticMeshInputLayout(blob);
	delete[] byteCode;

	return result;
}

bool Renderer::CreateStaticMeshInputLayout(BytecodeBlob& vertexShaderBytecode)
{
	OutputDebugString("Creating static mesh input layout...\n");

	HRESULT result = device->CreateInputLayout(GetStaticMeshInputElementDesc(), 
		STATIC_MESH_VERTEX_ATTRIBUTE_COUNT,
		vertexShaderBytecode.Bytecode, 
		vertexShaderBytecode.BytecodeLength, 
		&staticMeshInputLayout);

	if (FAILED(result))
		return false;

	return true;
}

bool Renderer::InitRenderObjects()
{
	OutputDebugString("Creating depth stencil state...\n");

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;

	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	HRESULT result = device->CreateDepthStencilState(&depthStencilDesc, &forwardPassDepthStencilState);

	if (FAILED(result))
		return false;

	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.StencilEnable = false;

	result = device->CreateDepthStencilState(&depthStencilDesc, &blitDepthStencilState);

	if (FAILED(result))
		return false;

	OutputDebugString("Creating raster state...\n");

	D3D11_RASTERIZER_DESC rasterDesc;
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	result = device->CreateRasterizerState(&rasterDesc, &forwardPassRasterState);

	if (FAILED(result))
		return false;

	OutputDebugString("Creating linear sampler state...\n");

	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	result = device->CreateSamplerState(&samplerDesc, &linearSamplerState);

	if (FAILED(result))
		return false;

	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	result = device->CreateSamplerState(&samplerDesc, &blitSamplerState);

	if (FAILED(result))
		return false;

	return true;
}

bool Renderer::InitInternalShaders()
{	
	bool result = internalContent->LoadPixelShader(BLIT_PIXEL_SHADER_LOCATION, &blitPixelShader);

	if (!result)
		return false;

	BytecodeBlob vertexShaderBytecode;
	result = internalContent->LoadVertexShader(BLIT_VERTEX_SHADER_LOCATION,
		&blitVertexShader, &vertexShaderBytecode);

	if (!result)
		return false;

	HRESULT hr = device->CreateInputLayout(GetBlitInputElementDesc(), BLIT_ATTRIBUTE_COUNT,
		vertexShaderBytecode.Bytecode, vertexShaderBytecode.BytecodeLength,
		&blitInputLayout);
	vertexShaderBytecode.Destroy();

	if (FAILED(hr))
		return false;

	return true;
}

bool Renderer::InitInternalVertexBuffers()
{
	float blitBufferData[] = 
	{
		-1.0f, 1.0f,
		0.0f, 0.0f,

		1.0f, 1.0f,
		1.0f, 0.0f,

		-1.0f, -1.0f,
		0.0f, 1.0f,

		1.0f, -1.0f,
		1.0f, 1.0f
	};

	D3D11_BUFFER_DESC blitBufferDesc;
	blitBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	blitBufferDesc.ByteWidth = sizeof(blitBufferData);
	blitBufferDesc.CPUAccessFlags = 0;
	blitBufferDesc.MiscFlags = 0;
	blitBufferDesc.StructureByteStride = 0;
	blitBufferDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA subData;
	ZeroMemory(&subData, sizeof(subData));
	subData.pSysMem = blitBufferData;

	HRESULT hr = device->CreateBuffer(&blitBufferDesc, &subData, &blitVertexBuffer);
	if (FAILED(hr))
		return false;

	return true;
}

void Renderer::DestroyRenderTarget()
{
	SAFE_RELEASE(defaultDepthStencilView);
	SAFE_RELEASE(depthStencilTexture);
	SAFE_RELEASE(backBufferRenderTarget);
}

void Renderer::DestroyDeferredTargets()
{
	for (auto resourceView : deferredShaderResourceViews)
		resourceView->Release();
	for (auto targetView : deferredRenderTargets)
		targetView->Release();
	for (auto targetBuffer : deferredBuffers)
		targetBuffer->Release();

	deferredShaderResourceClear.clear();
	deferredShaderResourceViews.clear();
	deferredRenderTargets.clear();
	deferredBuffers.clear();
}

bool Renderer::Reset(const RenderParams& params)
{
	if (params.Extent.Width == renderParameters.Extent.Width &&
		params.Extent.Height == renderParameters.Extent.Height &&
		params.Windowed == renderParameters.Windowed)
		return true;

	if (params.Extent.Width == 0 || params.Extent.Height == 0)
		return false;

	if (!bDisposed)
	{
		DestroyRenderTarget();
		DestroyDeferredTargets();

		if (renderParameters.Windowed != params.Windowed)
			swapChain->SetFullscreenState(!params.Windowed, nullptr);

		renderParameters = params;

		HRESULT result = swapChain->ResizeBuffers(2, renderParameters.Extent.Width,
			renderParameters.Extent.Height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		if (FAILED(result))
			return false;

		if (!InitRenderTarget())
			return false;

		return true;
	}
	else
		return false;
}

void Renderer::CollectVisibleStaticMeshes(RegionNode* node, const Frustum& cameraFrustum, vector<SceneNode*>& meshes)
{
	if (IsOutsideFrustum(node->AABB, cameraFrustum))
		return;

	if (node->LeafData != nullptr)
	{
		if (node->LeafData->IsZone())
			CollectVisibleStaticMeshes(node->LeafData, cameraFrustum, meshes);
		else if (node->LeafData->IsMesh())
			meshes.push_back(node->LeafData);
	}

	if (node->Node1 != nullptr)
		CollectVisibleStaticMeshes(node->Node1, cameraFrustum, meshes);
	if (node->Node2 != nullptr)
		CollectVisibleStaticMeshes(node->Node2, cameraFrustum, meshes);
	if (node->Node3 != nullptr)
		CollectVisibleStaticMeshes(node->Node3, cameraFrustum, meshes);
}

void Renderer::CollectVisibleStaticMeshes(SceneNode* node, const Frustum& cameraFrustum, vector<SceneNode*>& meshes)
{
	if (!node->IsZone())
	{
		OutputDebugString("Attempted static mesh collection on non-zone node!\n");
		return;
	}

	CollectVisibleStaticMeshes(&node->Region, cameraFrustum, meshes);
}

void Renderer::RenderPass(SceneNode* sceneRoot, ICamera* camera, const RenderPassType passType,
	D3D11_VIEWPORT& viewport, ID3D11RenderTargetView** renderTargets, const size_t renderTargetsCount,
	ID3D11DepthStencilView* depthStencilView)
{
	// Compute the camera frustum
	Frustum cameraFrustum;
	camera->GetFrustum(&cameraFrustum, renderParameters.Extent);

	// Get camera position
	XMFLOAT3 cameraPosition;
	camera->GetPosition(&cameraPosition);
	XMVECTOR cameraPositionVec = XMLoadFloat3(&cameraPosition);

	// Set render state
	deviceContext->RSSetViewports(1, &viewport);
	deviceContext->OMSetRenderTargets(renderTargetsCount, renderTargets, depthStencilView);
	deviceContext->OMSetDepthStencilState(forwardPassDepthStencilState, 0);
	deviceContext->RSSetState(forwardPassRasterState);

	// Prepare the transform data uniform buffer
	ID3D11Buffer* transformBuffer;
	D3D11_BUFFER_DESC transformBufferDesc;
	transformBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	transformBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	transformBufferDesc.ByteWidth = sizeof(XMFLOAT4X4) * 2;
	transformBufferDesc.CPUAccessFlags = 0;
	transformBufferDesc.MiscFlags = 0;
	transformBufferDesc.StructureByteStride = 0;

	XMFLOAT4X4 transforms[2];
	camera->GetViewMatrix(&transforms[0]);
	camera->GetProjectionMatrix(&transforms[1], renderParameters.Extent);

	D3D11_SUBRESOURCE_DATA transformData;
	ZeroMemory(&transformData, sizeof(transformData));
	transformData.pSysMem = transforms;

	HRESULT result = device->CreateBuffer(&transformBufferDesc, &transformData, &transformBuffer);
	if (FAILED(result))
		OutputDebugString("Failed to create transform buffer!\n");

	// Set primitive topology and input layout for static meshes
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->IASetInputLayout(staticMeshInputLayout);

	// Collect all of the visible meshes
	vector<SceneNode*> staticMeshes;
	CollectVisibleStaticMeshes(sceneRoot, cameraFrustum, staticMeshes);

	auto compareDistance = [&camera, &cameraPositionVec](SceneNode* n1, SceneNode* n2)
	{
		float d1;
		float d2;
		XMStoreFloat(&d1, XMVector3LengthSq(cameraPositionVec - 0.5f *
			(XMLoadFloat3(&n1->Region.AABB.Lower) +
			XMLoadFloat3(&n1->Region.AABB.Upper))));
		XMStoreFloat(&d2, XMVector3LengthSq(cameraPositionVec - 0.5f *
			(XMLoadFloat3(&n2->Region.AABB.Lower) +
			XMLoadFloat3(&n2->Region.AABB.Upper))));
		return d1 < d2;
	};

	auto isOpaque = [](SceneNode* n)
	{
		return !n->Material->IsTransparent;
	};

	auto compareMaterials = [](SceneNode* n1, SceneNode* n2)
	{
		return n1->Material < n2->Material;
	};

	auto compareMeshes = [](SceneNode* n1, SceneNode* n2)
	{
		return n1->Ref.StaticMesh < n2->Ref.StaticMesh;
	};

	// Reorder the visible meshes for batching
	sort(staticMeshes.begin(), staticMeshes.end(), compareDistance);
	stable_sort(staticMeshes.begin(), staticMeshes.end(), compareMeshes);
	stable_sort(staticMeshes.begin(), staticMeshes.end(), compareMaterials);
	stable_partition(staticMeshes.begin(), staticMeshes.end(), isOpaque);

	auto it = staticMeshes.begin();
	auto endIt = staticMeshes.end();

	while (it != endIt)
	{
		// Begin material for this batch
		auto currentMaterial = (*it)->Material;
		auto endMaterialIt = upper_bound(it, endIt, *it, compareMaterials);

		// Set material stage parameters
		deviceContext->VSSetShader(currentMaterial->VertexShader, nullptr, 0);
		deviceContext->PSSetShader(currentMaterial->PixelShader, nullptr, 0);

		if (currentMaterial->Type == MATERIAL_TYPE_STANDARD)
		{
			deviceContext->VSSetConstantBuffers(0, 1, &transformBuffer);

			deviceContext->PSSetSamplers(0, 1, &linearSamplerState);
			deviceContext->PSSetShaderResources(0, currentMaterial->PixelResourceViewCount,
				currentMaterial->PixelResourceViews);
			deviceContext->PSSetConstantBuffers(0, currentMaterial->PixelConstantBuffersCount,
				currentMaterial->PixelConstantBuffers);
		}

		while (it != endMaterialIt)
		{
			// Begin mesh for this batch
			auto currentMesh = (*it)->Ref.StaticMesh;
			auto endMeshIt = upper_bound(it, endMaterialIt, *it, compareMeshes);
			auto vertexBuffer = currentMesh->GetVertexBuffer();
			auto indexBuffer = currentMesh->GetIndexBuffer();
			UINT stride = STATIC_MESH_STRIDE;
			UINT offset = 0;

			// Bind mesh vertex and index buffers
			deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
			deviceContext->IASetIndexBuffer(indexBuffer, currentMesh->GetIndexFormat(), 0);

			// Collect instance transformation
			instanceCache.Clear();

			for (; it != endMeshIt; ++it)
				instanceCache.Push((*it)->Transform.Global);

			// Create transformation instance vertex buffer
			ID3D11Buffer* instanceBuffer;

			D3D11_BUFFER_DESC bufferDesc;
			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.ByteWidth = sizeof(XMFLOAT4X4) * instanceCache.GetSize();
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA subData;
			ZeroMemory(&subData, sizeof(subData));
			subData.pSysMem = instanceCache.GetData();

			result = device->CreateBuffer(&bufferDesc, &subData, &instanceBuffer);
			if (FAILED(result))
				OutputDebugString("Failed to create instance buffer!\n");

			// Bind the created buffer
			UINT instanceStride = sizeof(XMFLOAT4X4);
			deviceContext->IASetVertexBuffers(1, 1, &instanceBuffer, &instanceStride, &offset);

			// Draw instances
			deviceContext->DrawIndexedInstanced(currentMesh->GetIndexCount(), instanceCache.GetSize(), 0, 0, 0);

			// Clean up
			instanceBuffer->Release();
		}
	}

	transformBuffer->Release();
}

void Renderer::RenderFrame(SceneNode* sceneRoot, ICamera* camera)
{
	++frameCount;
	FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Clear depth and color attachments
	if (camera == nullptr)
	{
		OutputDebugString("Warning - Camera was set to nullptr!\n");
		return;
	}

	if (sceneRoot != nullptr)
	{
		// Prepare the viewport
		D3D11_VIEWPORT viewport;
		viewport.Width = (FLOAT)renderParameters.Extent.Width;
		viewport.Height = (FLOAT)renderParameters.Extent.Height;
		viewport.MaxDepth = 1.0f;
		viewport.MinDepth = 0.0f;
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;

		deviceContext->ClearRenderTargetView(backBufferRenderTarget, clearColor);
		deviceContext->ClearDepthStencilView(defaultDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		RenderPass(sceneRoot, camera, RENDER_PASS_TYPE_DEFERRED, viewport,
			&backBufferRenderTarget, 1, defaultDepthStencilView);
	}

	if (renderParameters.UseVSync)
		swapChain->Present(1, 0);
	else
		swapChain->Present(0, 0);
}

void Renderer::OnResize()
{
	swapChain->Present(0, 0);
}

void Renderer::Destroy()
{
	OutputDebugString("Disposing renderer objects...\n");

	bDisposed = true;

	// Delete the internal content
	if (internalContent != nullptr)
	{
		internalContent->Destroy();
		delete internalContent;
	}

	if (swapChain != nullptr && IsFullscreen())
		swapChain->SetFullscreenState(FALSE, nullptr);

	SAFE_RELEASE(blitVertexBuffer);
	SAFE_RELEASE(blitInputLayout);
	SAFE_RELEASE(staticMeshInputLayout);
	SAFE_RELEASE(linearSamplerState);
	SAFE_RELEASE(forwardPassRasterState);
	SAFE_RELEASE(forwardPassDepthStencilState);
	SAFE_RELEASE(blitDepthStencilState);
	SAFE_RELEASE(blitSamplerState);

	DestroyDeferredTargets();
	DestroyRenderTarget();

	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(deviceContext);
	SAFE_RELEASE(device);
}