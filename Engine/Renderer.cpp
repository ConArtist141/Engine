#include "Renderer.h"
#include "Camera.h"
#include "SceneGraph.h"
#include "ContentPackage.h"
#include "GraphicsDebug.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;
using namespace DirectX;

#define CAMERA_CONSTANT_BUFFER_SIZE sizeof(XMMATRIX) * 2
#define TERRAIN_PATCH_INSTANCE_CONSTANT_BUFFER_SIZE sizeof(XMMATRIX)
#define STATIC_MESH_INSTANCE_CONSTANT_BUFFER_SIZE sizeof(XMMATRIX)

#define SAFE_RELEASE(x) if (x != nullptr) x->Release();

bool CompareMaterials(SceneNode* n1, SceneNode* n2)
{
	return n1->MaterialData < n2->MaterialData;
}

bool CompareMeshes(SceneNode* n1, SceneNode* n2)
{
	return n1->Ref.StaticMesh < n2->Ref.StaticMesh;
}

template <typename CacheData>
inline size_t ResizingCache<CacheData>::GetSize() const
{
	return cacheSize;
}
template <typename CacheData>
inline size_t ResizingCache<CacheData>::GetReservedSize() const
{
	return cacheReserved;
}
template <typename CacheData>
inline CacheData* ResizingCache<CacheData>::GetData() const
{
	return cache;
}
template <typename CacheData>
ResizingCache<CacheData>::ResizingCache(const size_t reserve) :
	cache(new CacheData[reserve]), cacheReserved(reserve), cacheSize(0)
{ }

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
	forwardRenderTarget(nullptr),
	forwardDepthStencilView(nullptr),
	forwardDepthStencilTexture(nullptr),
	defaultDepthStencilState(nullptr),
	blitDepthStencilState(nullptr),
	defaultRasterState(nullptr),
	wireframeRasterState(nullptr),
	samplerStateLinearStaticMesh(nullptr),
	inputLayoutTerrainPatch(nullptr),
	inputLayoutStaticMesh(nullptr),
	inputLayoutStaticMeshInstanced(nullptr),
	pixelShaderDeferredComposite(nullptr),
	vertexShaderBlit(nullptr),
	pixelShaderStaticMesh(nullptr),
	vertexShaderStaticMeshInstanced(nullptr),
	inputLayoutBlit(nullptr),
	bufferBlitVertices(nullptr),
	bufferStaticMeshInstanceConstants(nullptr),
	bufferCameraConstants(nullptr),
	bufferTerrainPatchInstanceConstants(nullptr),
	samplerStateBlit(nullptr),
	internalContent(nullptr),
	deferredDepthStencilBuffer(nullptr),
	deferredDepthShaderView(nullptr),
	deferredDepthStencilView(nullptr),
	lightRenderTarget(nullptr),
	lightShaderView(nullptr),
	lightTexture(nullptr),
	frameCount(0),
	instanceCache(DEFAULT_INSTANCE_CACHE_SIZE)
{
	GetInputElementLayoutStaticMesh(&elementLayoutStaticMesh);
	GetInputElementLayoutStaticMeshInstanced(&elementLayoutStaticMeshInstanced);
	GetInputElementLayoutBlit(&elementLayoutBlit);
	GetInputElementLayoutTerrainPatch(&elementLayoutTerrainPatch);

	InitParameters.bLoadTerrainPatchShaders = true;
}

bool Renderer::Initialize(HWND hWindow, const RenderParams& params)
{
	bool result;
	result = InitWindow(hWindow, params);
	if (!result)
		return false;

	result = InitRenderTarget();
	if (!result)
		return false;

	result = InitDeferredTargets();
	if (!result)
		return false;

	result = InitRenderObjects();
	if (!result)
		return false;

	internalContent = new ContentPackage(this);

	result = InitInternalShaders();
	if (!result)
		return false;

	result = InitInternalVertexBuffers();
	if (!result)
		return false;

	result = InitConstantBuffers();
	if (!result)
		return false;

	NameObjectsDebug();

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
	result = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM,
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
	result = device->CreateRenderTargetView(backBuffer, nullptr, &forwardRenderTarget);
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

	result = device->CreateTexture2D(&depthTextureDesc, nullptr, &forwardDepthStencilTexture);

	if (FAILED(result))
		return false;

	OutputDebugString("Creating depth stencil view...\n");

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	result = device->CreateDepthStencilView(forwardDepthStencilTexture, &depthStencilViewDesc, &forwardDepthStencilView);

	if (FAILED(result))
		return false;

	return true;
}

bool Renderer::InitDeferredTargets()
{
	deferredBufferFormats =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM
	};

	HRESULT result;

	for (auto format : deferredBufferFormats)
	{
		D3D11_TEXTURE2D_DESC deferredBufferDesc;
		deferredBufferDesc.ArraySize = 1;
		deferredBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		deferredBufferDesc.CPUAccessFlags = 0;
		deferredBufferDesc.MipLevels = 1;
		deferredBufferDesc.MiscFlags = 0;
		deferredBufferDesc.SampleDesc.Count = 1;
		deferredBufferDesc.SampleDesc.Quality = 0;
		deferredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		deferredBufferDesc.Width = renderParameters.Extent.Width;
		deferredBufferDesc.Height = renderParameters.Extent.Height;
		deferredBufferDesc.Format = format;

		ID3D11Texture2D* deferredBuffer;
		result = device->CreateTexture2D(&deferredBufferDesc, nullptr, &deferredBuffer);
		if (FAILED(result))
			return false;

		deferredBuffers.push_back(deferredBuffer);

		ID3D11ShaderResourceView* resourceView;
		result = device->CreateShaderResourceView(deferredBuffer, nullptr, &resourceView);
		if (FAILED(result))
			return false;

		deferredShaderViews.push_back(resourceView);

		ID3D11RenderTargetView* renderTargetView;
		result = device->CreateRenderTargetView(deferredBuffer, nullptr, &renderTargetView);
		if (FAILED(result))
			return false;

		deferredRenderTargets.push_back(renderTargetView);

#if defined(ENABLE_DIRECT3D_DEBUG) && defined(ENABLE_NAMED_OBJECTS)
		SetDebugObjectName(deferredBuffer, "Deferred Buffer");
		SetDebugObjectName(resourceView, "Deferred Shader Resource View");
		SetDebugObjectName(renderTargetView, "Deferred Render Target View");
#endif
	}

	DXGI_FORMAT depthTextureFormat = DXGI_FORMAT_R32_TYPELESS;
	DXGI_FORMAT depthResourceViewFormat = DXGI_FORMAT_R32_FLOAT;
	DXGI_FORMAT depthViewFormat = DXGI_FORMAT_D32_FLOAT;

	D3D11_TEXTURE2D_DESC deferredBufferDesc;
	deferredBufferDesc.ArraySize = 1;
	deferredBufferDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	deferredBufferDesc.CPUAccessFlags = 0;
	deferredBufferDesc.MipLevels = 1;
	deferredBufferDesc.MiscFlags = 0;
	deferredBufferDesc.SampleDesc.Count = 1;
	deferredBufferDesc.SampleDesc.Quality = 0;
	deferredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	deferredBufferDesc.Width = renderParameters.Extent.Width;
	deferredBufferDesc.Height = renderParameters.Extent.Height;
	deferredBufferDesc.Format = depthTextureFormat;

	result = device->CreateTexture2D(&deferredBufferDesc, nullptr, &deferredDepthStencilBuffer);

	if (FAILED(result))
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
	ZeroMemory(&resourceViewDesc, sizeof(resourceViewDesc));
	resourceViewDesc.Format = depthResourceViewFormat;
	resourceViewDesc.Texture2D.MipLevels = 1;
	resourceViewDesc.Texture2D.MostDetailedMip = 0;
	resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

	result = device->CreateShaderResourceView(deferredDepthStencilBuffer, &resourceViewDesc, &deferredDepthShaderView);

	if (FAILED(result))
		return false;

	D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
	ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));

	depthStencilViewDesc.Format = depthViewFormat;
	depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depthStencilViewDesc.Texture2D.MipSlice = 0;

	result = device->CreateDepthStencilView(deferredDepthStencilBuffer, &depthStencilViewDesc, &deferredDepthStencilView);

	if (FAILED(result))
		return false;

#if defined(ENABLE_DIRECT3D_DEBUG) && defined(ENABLE_NAMED_OBJECTS)
	SetDebugObjectName(deferredDepthStencilBuffer, "Deferred Depth Stencil Buffer");
	SetDebugObjectName(deferredDepthShaderView, "Deferred Depth Stencil Shader Resource View");
	SetDebugObjectName(deferredDepthStencilView, "Deferred Depth Stencil View");
#endif

	DXGI_FORMAT lightFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	D3D11_TEXTURE2D_DESC lightBufferDesc;
	lightBufferDesc.ArraySize = 1;
	lightBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	lightBufferDesc.CPUAccessFlags = 0;
	lightBufferDesc.MipLevels = 1;
	lightBufferDesc.MiscFlags = 0;
	lightBufferDesc.SampleDesc.Count = 1;
	lightBufferDesc.SampleDesc.Quality = 0;
	lightBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	lightBufferDesc.Width = renderParameters.Extent.Width;
	lightBufferDesc.Height = renderParameters.Extent.Height;
	lightBufferDesc.Format = lightFormat;

	result = device->CreateTexture2D(&lightBufferDesc, nullptr, &lightTexture);

	if (FAILED(result))
		return false;

	result = device->CreateRenderTargetView(lightTexture, nullptr, &lightRenderTarget);

	if (FAILED(result))
		return false;

	result = device->CreateShaderResourceView(lightTexture, nullptr, &lightShaderView);

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

	HRESULT result = device->CreateDepthStencilState(&depthStencilDesc, &defaultDepthStencilState);

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

	result = device->CreateRasterizerState(&rasterDesc, &defaultRasterState);

	if (FAILED(result))
		return false;

	rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
	result = device->CreateRasterizerState(&rasterDesc, &wireframeRasterState);

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

	result = device->CreateSamplerState(&samplerDesc, &samplerStateLinearStaticMesh);

	if (FAILED(result))
		return false;

	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	result = device->CreateSamplerState(&samplerDesc, &samplerStateBlit);

	if (FAILED(result))
		return false;

	return true;
}

bool Renderer::InitInternalShaders()
{
	bool result;
	HRESULT hr;

	BytecodeBlob staticMeshBytecode;
	result = internalContent->LoadVertexShader(STATIC_MESH_VERTEX_SHADER_LOCATION, &vertexShaderStaticMesh,
		&staticMeshBytecode);

	if (!result)
		return false;

	hr = device->CreateInputLayout(elementLayoutStaticMesh.Desc, elementLayoutStaticMesh.AttributeCount,
		staticMeshBytecode.Bytecode, staticMeshBytecode.BytecodeLength,
		&inputLayoutStaticMesh);
	staticMeshBytecode.Destroy();

	if (FAILED(hr))
		return false;

	// Load instanced static mesh shaders
	BytecodeBlob staticMeshInstancedBytecode;
	result = internalContent->LoadVertexShader(STATIC_MESH_INSTANCED_VERTEX_SHADER_LOCATION, &vertexShaderStaticMeshInstanced,
		&staticMeshInstancedBytecode);

	if (!result)
		return false;

	hr = device->CreateInputLayout(elementLayoutStaticMeshInstanced.Desc, elementLayoutStaticMeshInstanced.AttributeCount,
		staticMeshInstancedBytecode.Bytecode, staticMeshInstancedBytecode.BytecodeLength,
		&inputLayoutStaticMeshInstanced);
	staticMeshInstancedBytecode.Destroy();

	if (FAILED(hr))
		return false;

	result = internalContent->LoadPixelShader(STATIC_MESH_PIXEL_SHADER_LOCATION, &pixelShaderStaticMesh);

	if (!result)
		return false;

	// Load blit shaders
	result = internalContent->LoadPixelShader(DEFERRED_COMPOSITE_PIXEL_SHADER_LOCATION, &pixelShaderDeferredComposite);

	if (!result)
		return false;

	BytecodeBlob vertexShaderBytecode;
	result = internalContent->LoadVertexShader(BLIT_VERTEX_SHADER_LOCATION, &vertexShaderBlit, &vertexShaderBytecode);

	if (!result)
		return false;

	hr = device->CreateInputLayout(elementLayoutBlit.Desc, elementLayoutBlit.AttributeCount,
		vertexShaderBytecode.Bytecode, vertexShaderBytecode.BytecodeLength,
		&inputLayoutBlit);
	vertexShaderBytecode.Destroy();

	if (FAILED(hr))
		return false;

	// Load terrain patch shaders
	if (InitParameters.bLoadTerrainPatchShaders)
	{
		BytecodeBlob terrainPatchBytecode;
		result = internalContent->LoadVertexShader(TERRAIN_PATCH_VERTEX_SHADER_LOCATION, &vertexShaderTerrainPatch,
			&terrainPatchBytecode);

		if (!result)
			return false;

		hr = device->CreateInputLayout(elementLayoutTerrainPatch.Desc, elementLayoutTerrainPatch.AttributeCount,
			terrainPatchBytecode.Bytecode, terrainPatchBytecode.BytecodeLength,
			&inputLayoutTerrainPatch);
		terrainPatchBytecode.Destroy();

		if (FAILED(hr))
			return false;

		result = internalContent->LoadPixelShader(TERRAIN_PATCH_PIXEL_SHADER_LOCATION, &pixelShaderTerrainPatch);

		if (!result)
			return false;
	}

	return true;
}

bool Renderer::InitConstantBuffers()
{
	D3D11_BUFFER_DESC cameraBufferDesc;
	ZeroMemory(&cameraBufferDesc, sizeof(cameraBufferDesc));
	cameraBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cameraBufferDesc.ByteWidth = CAMERA_CONSTANT_BUFFER_SIZE;
	cameraBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cameraBufferDesc.Usage = D3D11_USAGE_DYNAMIC;

	HRESULT result = device->CreateBuffer(&cameraBufferDesc, nullptr, &bufferCameraConstants);
	if (FAILED(result))
		return false;

	D3D11_BUFFER_DESC terrainPatchBufferDesc;
	ZeroMemory(&terrainPatchBufferDesc, sizeof(terrainPatchBufferDesc));
	terrainPatchBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	terrainPatchBufferDesc.ByteWidth = TERRAIN_PATCH_INSTANCE_CONSTANT_BUFFER_SIZE;
	terrainPatchBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	terrainPatchBufferDesc.Usage = D3D11_USAGE_DYNAMIC;

	result = device->CreateBuffer(&terrainPatchBufferDesc, nullptr, &bufferTerrainPatchInstanceConstants);
	if (FAILED(result))
		return false;

	D3D11_BUFFER_DESC staticMeshBufferDesc = terrainPatchBufferDesc;
	staticMeshBufferDesc.ByteWidth = STATIC_MESH_INSTANCE_CONSTANT_BUFFER_SIZE;

	result = device->CreateBuffer(&staticMeshBufferDesc, nullptr, &bufferStaticMeshInstanceConstants);
	if (FAILED(result))
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

		-1.0f, -1.0f,
		0.0f, 1.0f,

		1.0f, 1.0f,
		1.0f, 0.0f,

		1.0f, -1.0f,
		1.0f, 1.0f
	};

	D3D11_BUFFER_DESC blitBufferDesc;
	blitBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	blitBufferDesc.ByteWidth = sizeof(blitBufferData);
	blitBufferDesc.CPUAccessFlags = 0;
	blitBufferDesc.MiscFlags = 0;
	blitBufferDesc.StructureByteStride = 0;
	blitBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;

	D3D11_SUBRESOURCE_DATA subData;
	ZeroMemory(&subData, sizeof(subData));
	subData.pSysMem = blitBufferData;

	HRESULT hr = device->CreateBuffer(&blitBufferDesc, &subData, &bufferBlitVertices);
	if (FAILED(hr))
		return false;

	return true;
}

void Renderer::NameObjectsDebug()
{
#if defined(ENABLE_DIRECT3D_DEBUG) && defined(ENABLE_NAMED_OBJECTS)
	SetDebugObjectName(forwardRenderTarget, "Back Buffer Render Target");

	SetDebugObjectName(bufferBlitVertices, "Blit Vertices Vertex Buffer");
	SetDebugObjectName(bufferCameraConstants, "Camera Constants Buffer");
	SetDebugObjectName(bufferStaticMeshInstanceConstants, "Static Mesh Instance Constant Buffer");
	SetDebugObjectName(bufferTerrainPatchInstanceConstants, "Terrain Patch Instance Constant Buffer");

	SetDebugObjectName(vertexShaderBlit, "Blit Vertex Shader");
	SetDebugObjectName(vertexShaderStaticMesh, "Static Mesh Vertex Shader");
	SetDebugObjectName(vertexShaderStaticMeshInstanced, "Instanced Static Mesh Vertex Shader");
	SetDebugObjectName(vertexShaderTerrainPatch, "Terrain Patch Vertex Shader");
	SetDebugObjectName(pixelShaderDeferredComposite, "Blit Pixel Shader");
	SetDebugObjectName(pixelShaderStaticMesh, "Static Mesh Pixel Shader");
	SetDebugObjectName(pixelShaderTerrainPatch, "Terrain Patch Pixel Shader");

	SetDebugObjectName(forwardDepthStencilView, "Default Depth Stencil View");
	SetDebugObjectName(forwardDepthStencilTexture, "Depth Stencil Texture");
	SetDebugObjectName(defaultDepthStencilState, "Forward Pass Depth Stencil State");
	SetDebugObjectName(defaultRasterState, "Forward Pass Raster State");
	SetDebugObjectName(wireframeRasterState, "Wireframe Raster State");
	SetDebugObjectName(samplerStateLinearStaticMesh, "Linear Static Mesh Sampler");
	SetDebugObjectName(samplerStateBlit, "Blit Sampler");
	SetDebugObjectName(samplerStateTerrainPatch, "Terrain Patch Sampler");

	SetDebugObjectName(inputLayoutStaticMesh, "Static Mesh Layout");
	SetDebugObjectName(inputLayoutStaticMeshInstanced, "Static Mesh Instanced Layout");
	SetDebugObjectName(inputLayoutBlit, "Blit Layout");
	SetDebugObjectName(inputLayoutTerrainPatch, "Terrain Patch Layout");
#endif
}

void Renderer::DestroyRenderTarget()
{
	SAFE_RELEASE(forwardDepthStencilView);
	SAFE_RELEASE(forwardDepthStencilTexture);
	SAFE_RELEASE(forwardRenderTarget);
}

void Renderer::DestroyDeferredTargets()
{
	for (auto resourceView : deferredShaderViews)
		resourceView->Release();
	for (auto targetView : deferredRenderTargets)
		targetView->Release();
	for (auto targetBuffer : deferredBuffers)
		targetBuffer->Release();

	deferredShaderViews.clear();
	deferredRenderTargets.clear();
	deferredBuffers.clear();

	SAFE_RELEASE(deferredDepthShaderView);
	SAFE_RELEASE(deferredDepthStencilView);
	SAFE_RELEASE(deferredDepthStencilBuffer);

	SAFE_RELEASE(lightRenderTarget);
	SAFE_RELEASE(lightShaderView);
	SAFE_RELEASE(lightTexture);
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

		if (!InitDeferredTargets())
			return false;

		return true;
	}
	else
		return false;
}

void Renderer::CollectVisibleNodes(RegionNode* node, const Frustum& cameraFrustum, NodeCollection& nodes)
{
	if (IsOutsideFrustum(node->AABB, cameraFrustum))
		return;

	if (node->LeafData != nullptr)
	{
		if (node->LeafData->IsZone())
			CollectVisibleNodes(node->LeafData, cameraFrustum, nodes);
		else if (node->LeafData->IsMesh())
		{
			if (node->LeafData->IsStaticMesh())
				nodes.StaticMeshes.push_back(node->LeafData);
			else if (node->LeafData->IsStaticMeshInstanced())
				nodes.InstancedStaticMeshes.push_back(node->LeafData);
			else if (node->LeafData->IsTerrainPatch())
				nodes.TerrainPatches.push_back(node->LeafData);
		}
	}

	if (node->Node1 != nullptr)
		CollectVisibleNodes(node->Node1, cameraFrustum, nodes);
	if (node->Node2 != nullptr)
		CollectVisibleNodes(node->Node2, cameraFrustum, nodes);
	if (node->Node3 != nullptr)
		CollectVisibleNodes(node->Node3, cameraFrustum, nodes);
}

void Renderer::CollectVisibleNodes(SceneNode* node, const Frustum& cameraFrustum, NodeCollection& nodes)
{
	if (!node->IsZone())
	{
		OutputDebugString("Attempted static mesh collection on non-zone node!\n");
		return;
	}

	CollectVisibleNodes(&node->Region, cameraFrustum, nodes);
}

void Renderer::DeferredRenderPass(SceneNode* sceneRoot, ICamera* camera,
	const vector<ID3D11RenderTargetView*>& renderTargets, ID3D11DepthStencilView* depthStencilView)
{
	// Deferred pass
	FLOAT clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTargets[0], clearColor);
	deviceContext->ClearDepthStencilView(deferredDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
	deviceContext->OMSetRenderTargets(renderTargets.size(), renderTargets.data(), depthStencilView);
	deviceContext->OMSetDepthStencilState(defaultDepthStencilState, 0);

	// Compute the camera frustum
	Frustum cameraFrustum;
	camera->GetFrustum(&cameraFrustum, renderParameters.Extent);

	// Prepare the camera transform data
	XMFLOAT4X4 transforms[2];
	camera->GetViewMatrix(&transforms[0]);
	camera->GetProjectionMatrix(&transforms[1], renderParameters.Extent);

	D3D11_MAPPED_SUBRESOURCE mappedSubRes;
	deviceContext->Map(bufferCameraConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubRes);
	memcpy(mappedSubRes.pData, transforms, sizeof(transforms));
	deviceContext->Unmap(bufferCameraConstants, 0);

	// Collect all of the visible meshes
	NodeCollection nodes;
	CollectVisibleNodes(sceneRoot, cameraFrustum, nodes);
	SortMeshNodes(nodes, camera);

	RenderStaticMeshes(nodes.StaticMeshes.begin(), nodes.StaticMeshes.end());
	RenderStaticMeshesInstanced(nodes.InstancedStaticMeshes.begin(), nodes.InstancedStaticMeshes.end());
	RenderTerrainPatches(nodes.TerrainPatches.begin(), nodes.TerrainPatches.end());
}

void Renderer::LightRenderPass(SceneNode* sceneRoot, ICamera* camera, 
	const std::vector<ID3D11ShaderResourceView*>& deferredResourceViews,
	ID3D11ShaderResourceView* deferredDepthResourceView, 
	ID3D11RenderTargetView* renderTarget)
{
	vector<ID3D11ShaderResourceView*> shaderResourceViews;
	for (size_t i = 0; i < deferredResourceViews.size(); ++i)
		shaderResourceViews.push_back(deferredResourceViews[i]);
	shaderResourceViews.push_back(deferredDepthResourceView);

	deviceContext->OMSetRenderTargets(1, &renderTarget, nullptr);
	deviceContext->OMSetDepthStencilState(blitDepthStencilState, 0);

	FLOAT color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTarget, color);
}

void Renderer::ForwardRenderPass(SceneNode* sceneRoot, ICamera* camera,
	const vector<ID3D11ShaderResourceView*>& deferredResourceViews, 
	ID3D11ShaderResourceView* deferredDepthResourceView,
	ID3D11ShaderResourceView* lightResourceView,
	ID3D11RenderTargetView* renderTarget,
	ID3D11DepthStencilView* depthStencilView)
{
	vector<ID3D11ShaderResourceView*> shaderResourceViews;
	for (size_t i = 0; i < deferredResourceViews.size(); ++i)
		shaderResourceViews.push_back(deferredResourceViews[i]);
	shaderResourceViews.push_back(deferredDepthResourceView);
	shaderResourceViews.push_back(lightResourceView);

	deviceContext->OMSetRenderTargets(1, &renderTarget, depthStencilView);
	deviceContext->OMSetDepthStencilState(blitDepthStencilState, 0);

	UINT stride = elementLayoutBlit.Stride;
	UINT offset = 0;

	deviceContext->VSSetShader(vertexShaderBlit, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderDeferredComposite, nullptr, 0);
	deviceContext->PSSetShaderResources(0, shaderResourceViews.size(), shaderResourceViews.data());
	deviceContext->PSSetSamplers(0, 1, &samplerStateBlit);

	deviceContext->IASetInputLayout(inputLayoutBlit);
	deviceContext->IASetVertexBuffers(0, 1, &bufferBlitVertices, &stride, &offset);

	deviceContext->Draw(6, 0);
}

void Renderer::ClearPixelShaderResources(const size_t resourceCount)
{
	unique_ptr<ID3D11ShaderResourceView*[]> resourceViews(new ID3D11ShaderResourceView*[resourceCount]);
	ZeroMemory(resourceViews.get(), sizeof(ID3D11ShaderResourceView*) * resourceCount);
	deviceContext->PSSetShaderResources(0, resourceCount, resourceViews.get());
}

void Renderer::RenderStaticMeshes(vector<SceneNode*>::iterator& begin, vector<SceneNode*>::iterator& end)
{
	// Set primitive topology and input layout for static meshes
	deviceContext->IASetInputLayout(inputLayoutStaticMesh);

	deviceContext->VSSetShader(vertexShaderStaticMesh, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderStaticMesh, nullptr, 0);
	deviceContext->PSSetSamplers(0, 1, &samplerStateLinearStaticMesh);

	ID3D11Buffer* vertexShaderConstantBuffers[] = { bufferCameraConstants, bufferStaticMeshInstanceConstants };
	deviceContext->VSSetConstantBuffers(0, 2, vertexShaderConstantBuffers);

	D3D11_MAPPED_SUBRESOURCE mappedSubRes;

	auto it = begin;

	while (it != end)
	{
		// Begin material for this batch
		auto currentMaterial = (*it)->MaterialData;
		auto endMaterialIt = upper_bound(it, end, *it, CompareMaterials);

		if (currentMaterial->Type == MATERIAL_TYPE_STANDARD)
		{
			if (currentMaterial->PixelResourceViews.size() > 0)
				deviceContext->PSSetShaderResources(0, currentMaterial->PixelResourceViews.size(), currentMaterial->PixelResourceViews.data());
			if (currentMaterial->PixelConstantBuffers.size() > 0)
				deviceContext->PSSetConstantBuffers(0, currentMaterial->PixelConstantBuffers.size(), currentMaterial->PixelConstantBuffers.data());
		}

		while (it != endMaterialIt)
		{
			// Begin mesh for this batch
			auto currentMesh = (*it)->Ref.StaticMesh;
			auto endMeshIt = upper_bound(it, endMaterialIt, *it, CompareMeshes);
			auto vertexBuffer = currentMesh->GetVertexBuffer();
			auto indexBuffer = currentMesh->GetIndexBuffer();
			UINT stride = elementLayoutStaticMesh.Stride;
			UINT offset = 0;

			// Bind mesh vertex and index buffers
			deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
			deviceContext->IASetIndexBuffer(indexBuffer, currentMesh->GetIndexFormat(), 0);

			for (; it != endMeshIt; ++it)
			{
				deviceContext->Map(bufferStaticMeshInstanceConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubRes);
				memcpy(mappedSubRes.pData, &(*it)->Transform.Global, sizeof((*it)->Transform.Global));
				deviceContext->Unmap(bufferStaticMeshInstanceConstants, 0);

				deviceContext->DrawIndexed(currentMesh->GetIndexCount(), 0, 0);
			}
		}
	}
}

void Renderer::RenderStaticMeshesInstanced(vector<SceneNode*>::iterator& begin, vector<SceneNode*>::iterator& end)
{
	// Set primitive topology and input layout for static meshes
	deviceContext->IASetInputLayout(inputLayoutStaticMeshInstanced);

	// Set material stage parameters
	deviceContext->VSSetShader(vertexShaderStaticMeshInstanced, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderStaticMesh, nullptr, 0);
	deviceContext->PSSetSamplers(0, 1, &samplerStateLinearStaticMesh);
	deviceContext->VSSetConstantBuffers(0, 1, &bufferCameraConstants);

	auto it = begin;

	while (it != end)
	{
		// Begin material for this batch
		auto currentMaterial = (*it)->MaterialData;
		auto endMaterialIt = upper_bound(it, end, *it, CompareMaterials);

		if (currentMaterial->Type == MATERIAL_TYPE_STANDARD)
		{
			if (currentMaterial->PixelResourceViews.size() > 0)
				deviceContext->PSSetShaderResources(0, currentMaterial->PixelResourceViews.size(), currentMaterial->PixelResourceViews.data());
			if (currentMaterial->PixelConstantBuffers.size() > 0)
				deviceContext->PSSetConstantBuffers(0, currentMaterial->PixelConstantBuffers.size(), currentMaterial->PixelConstantBuffers.data());
		}

		while (it != endMaterialIt)
		{
			// Begin mesh for this batch
			auto currentMesh = (*it)->Ref.StaticMesh;
			auto endMeshIt = upper_bound(it, endMaterialIt, *it, CompareMeshes);
			auto vertexBuffer = currentMesh->GetVertexBuffer();
			auto indexBuffer = currentMesh->GetIndexBuffer();
			UINT stride = elementLayoutStaticMeshInstanced.Stride;
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

			HRESULT result = device->CreateBuffer(&bufferDesc, &subData, &instanceBuffer);
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
}

void Renderer::RenderTerrainPatches(vector<SceneNode*>::iterator& begin, vector<SceneNode*>::iterator& end)
{
	// Set input layout for static meshes
	deviceContext->IASetInputLayout(inputLayoutTerrainPatch);

	deviceContext->VSSetShader(vertexShaderTerrainPatch, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderTerrainPatch, nullptr, 0);
	deviceContext->PSSetSamplers(0, 1, &samplerStateLinearStaticMesh);

	ID3D11Buffer* vertexShaderConstantBuffers[] = { bufferCameraConstants, bufferTerrainPatchInstanceConstants };
	deviceContext->VSSetConstantBuffers(0, 2, vertexShaderConstantBuffers);

	D3D11_MAPPED_SUBRESOURCE mappedSubRes;

	for (auto it = begin; it != end; ++it)
	{
		auto terrainNode = *it;

		UINT stride = elementLayoutTerrainPatch.Stride;
		UINT offset = 0;

		deviceContext->IASetVertexBuffers(0, 1, &terrainNode->Ref.TerrainPatch->MeshData.VertexBuffer, &stride, &offset);
		deviceContext->IASetIndexBuffer(terrainNode->Ref.TerrainPatch->MeshData.IndexBuffer, DXGI_FORMAT_R16_UINT, offset);
		deviceContext->PSSetShaderResources(0, 1, &terrainNode->Ref.TerrainPatch->MaterialData.Albedo);

		deviceContext->Map(bufferTerrainPatchInstanceConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubRes);
		memcpy(mappedSubRes.pData, &terrainNode->Transform.Global, sizeof(terrainNode->Transform.Global));
		deviceContext->Unmap(bufferTerrainPatchInstanceConstants, 0);

		deviceContext->DrawIndexed(terrainNode->Ref.TerrainPatch->MeshData.IndexCount, 0, 0);
	}
}

void Renderer::SortMeshNodes(NodeCollection& nodes, ICamera* camera)
{
	// Get camera position
	XMFLOAT3 cameraPosition;
	camera->GetPosition(&cameraPosition);
	XMVECTOR cameraPositionVec = XMLoadFloat3(&cameraPosition);

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
		return !n->MaterialData->IsTransparent;
	};

	auto compareLights = [](SceneNode* n1, SceneNode* n2)
	{
		return n1->Ref.LightData->Type < n2->Ref.LightData->Type;
	};

	// Reorder the visible meshes for batching
	auto sortStaticMeshCollection = [&compareDistance, &isOpaque](vector<SceneNode*>& collection)
	{
		sort(collection.begin(), collection.end(), compareDistance);
		stable_sort(collection.begin(), collection.end(), CompareMeshes);
		stable_sort(collection.begin(), collection.end(), CompareMaterials);
		stable_partition(collection.begin(), collection.end(), isOpaque);
	};

	sortStaticMeshCollection(nodes.StaticMeshes);
	sortStaticMeshCollection(nodes.InstancedStaticMeshes);

	sort(nodes.TerrainPatches.begin(), nodes.TerrainPatches.end(), compareDistance);
	sort(nodes.Lights.begin(), nodes.Lights.end(), compareLights);
}

void Renderer::RenderFrame(SceneNode* sceneRoot, ICamera* camera)
{
	++frameCount;

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

		// Set render state
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->RSSetViewports(1, &viewport);
		deviceContext->RSSetState(defaultRasterState);

		DeferredRenderPass(sceneRoot, camera, deferredRenderTargets, deferredDepthStencilView);
		LightRenderPass(sceneRoot, camera, deferredShaderViews, deferredDepthShaderView, 
			lightRenderTarget);
		ForwardRenderPass(sceneRoot, camera, deferredShaderViews, deferredDepthShaderView,
			lightShaderView, forwardRenderTarget, forwardDepthStencilView);
		ClearPixelShaderResources(deferredShaderViews.size() + 2);
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

	SAFE_RELEASE(bufferBlitVertices);
	SAFE_RELEASE(bufferCameraConstants);
	SAFE_RELEASE(bufferTerrainPatchInstanceConstants);
	SAFE_RELEASE(bufferStaticMeshInstanceConstants);
	SAFE_RELEASE(inputLayoutBlit);
	SAFE_RELEASE(inputLayoutStaticMesh);
	SAFE_RELEASE(inputLayoutStaticMeshInstanced);
	SAFE_RELEASE(inputLayoutTerrainPatch);
	SAFE_RELEASE(samplerStateLinearStaticMesh);
	SAFE_RELEASE(defaultRasterState);
	SAFE_RELEASE(wireframeRasterState);
	SAFE_RELEASE(defaultDepthStencilState);
	SAFE_RELEASE(blitDepthStencilState);
	SAFE_RELEASE(samplerStateBlit);

	DestroyDeferredTargets();
	DestroyRenderTarget();

	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(deviceContext);
	SAFE_RELEASE(device);
}