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
	backBufferRenderTarget(nullptr),
	defaultDepthStencilView(nullptr),
	depthStencilTexture(nullptr),
	forwardPassDepthStencilState(nullptr),
	blitDepthStencilState(nullptr),
	forwardPassRasterState(nullptr),
	wireframeRasterState(nullptr),
	samplerStateLinearStaticMesh(nullptr),
	inputLayoutTerrainPatch(nullptr),
	inputLayoutStaticMesh(nullptr),
	inputLayoutStaticMeshInstanced(nullptr),
	pixelShaderBlit(nullptr),
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
	frameCount(0),
	instanceCache(DEFAULT_INSTANCE_CACHE_SIZE)
{
	GetInputElementLayoutStaticMesh(&elementLayoutStaticMesh);
	GetInputElementLayoutStaticMeshInstanced(&elementLayoutStaticMeshInstanced);
	GetInputElementLayoutBlit(&elementLayoutBlit);
	GetInputElementLayoutTerrainPatch(&elementLayoutTerrainPatch);

	InitParameters.bLoadBlitShaders = false;
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
	if (InitParameters.bLoadBlitShaders)
	{
		result = internalContent->LoadPixelShader(BLIT_PIXEL_SHADER_LOCATION, &pixelShaderBlit);

		if (!result)
			return false;

		BytecodeBlob vertexShaderBytecode;
		result = internalContent->LoadVertexShader(BLIT_VERTEX_SHADER_LOCATION,
			&vertexShaderBlit, &vertexShaderBytecode);

		if (!result)
			return false;

		hr = device->CreateInputLayout(elementLayoutBlit.Desc, elementLayoutBlit.AttributeCount,
			vertexShaderBytecode.Bytecode, vertexShaderBytecode.BytecodeLength,
			&inputLayoutBlit);
		vertexShaderBytecode.Destroy();

		if (FAILED(hr))
			return false;
	}

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
	SetDebugObjectName(backBufferRenderTarget, "Back Buffer Render Target");
	
	SetDebugObjectName(bufferBlitVertices, "Blit Vertices Vertex Buffer");
	SetDebugObjectName(bufferCameraConstants, "Camera Constants Buffer");
	SetDebugObjectName(bufferStaticMeshInstanceConstants, "Static Mesh Instance Constant Buffer");
	SetDebugObjectName(bufferTerrainPatchInstanceConstants, "Terrain Patch Instance Constant Buffer");

	SetDebugObjectName(vertexShaderBlit, "Blit Vertex Shader");
	SetDebugObjectName(vertexShaderStaticMesh, "Static Mesh Vertex Shader");
	SetDebugObjectName(vertexShaderStaticMeshInstanced, "Instanced Static Mesh Vertex Shader");
	SetDebugObjectName(vertexShaderTerrainPatch, "Terrain Patch Vertex Shader");
	SetDebugObjectName(pixelShaderBlit, "Blit Pixel Shader");
	SetDebugObjectName(pixelShaderStaticMesh, "Static Mesh Pixel Shader");
	SetDebugObjectName(pixelShaderTerrainPatch, "Terrain Patch Pixel Shader");

	SetDebugObjectName(defaultDepthStencilView, "Default Depth Stencil View");
	SetDebugObjectName(depthStencilTexture, "Depth Stencil Texture");
	SetDebugObjectName(forwardPassDepthStencilState, "Forward Pass Depth Stencil State");
	SetDebugObjectName(forwardPassRasterState, "Forward Pass Raster State");
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

void Renderer::RenderPass(SceneNode* sceneRoot, ICamera* camera, const RenderPassType passType,
	D3D11_VIEWPORT& viewport, ID3D11RenderTargetView** renderTargets, const size_t renderTargetsCount,
	ID3D11DepthStencilView* depthStencilView)
{
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

	// Set render state
	deviceContext->RSSetViewports(1, &viewport);
	deviceContext->OMSetRenderTargets(renderTargetsCount, renderTargets, depthStencilView);
	deviceContext->OMSetDepthStencilState(forwardPassDepthStencilState, 0);
	deviceContext->RSSetState(forwardPassRasterState);

	// Collect all of the visible meshes
	NodeCollection nodes;
	CollectVisibleNodes(sceneRoot, cameraFrustum, nodes);
	SortMeshNodes(nodes, camera);

	RenderStaticMeshes(nodes.StaticMeshes.begin(), nodes.StaticMeshes.end());
	RenderStaticMeshesInstanced(nodes.InstancedStaticMeshes.begin(), nodes.InstancedStaticMeshes.end());
	RenderTerrainPatches(nodes.TerrainPatches.begin(), nodes.TerrainPatches.end());
}

void Renderer::RenderStaticMeshes(vector<SceneNode*>::iterator& begin, vector<SceneNode*>::iterator& end)
{
	// Set primitive topology and input layout for static meshes
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->IASetInputLayout(inputLayoutStaticMesh);

	deviceContext->VSSetShader(vertexShaderStaticMesh, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderStaticMesh, nullptr, 0);
	deviceContext->PSSetSamplers(0, 1, &samplerStateLinearStaticMesh);

	ID3D11Buffer* vertexShaderConstantBuffers[] = { bufferCameraConstants, bufferStaticMeshInstanceConstants };
	deviceContext->VSSetConstantBuffers(0, 2, &bufferCameraConstants);

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
				memcpy(mappedSubRes.pData, &(*it)->Transform.Global, sizeof(XMMATRIX));
				deviceContext->Unmap(bufferStaticMeshInstanceConstants, 0);

				deviceContext->DrawIndexed(currentMesh->GetIndexCount(), 0, 0);
			}
		}
	}
}

void Renderer::RenderStaticMeshesInstanced(vector<SceneNode*>::iterator& begin, vector<SceneNode*>::iterator& end)
{
	// Set primitive topology and input layout for static meshes
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
	// Set primitive topology and input layout for static meshes
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	deviceContext->IASetInputLayout(inputLayoutTerrainPatch);

	deviceContext->VSSetShader(vertexShaderTerrainPatch, nullptr, 0);
	deviceContext->PSSetShader(pixelShaderTerrainPatch, nullptr, 0);
	deviceContext->PSSetSamplers(0, 1, &samplerStateLinearStaticMesh);

	ID3D11Buffer* vertexShaderConstantBuffers[] = { bufferCameraConstants, bufferTerrainPatchInstanceConstants };
	deviceContext->VSSetConstantBuffers(0, 2, &bufferCameraConstants);

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
		memcpy(mappedSubRes.pData, &terrainNode->Transform.Global, sizeof(XMMATRIX));
		deviceContext->Unmap(bufferTerrainPatchInstanceConstants, 0);

		deviceContext->DrawIndexed(terrainNode->Ref.TerrainPatch->MeshData.IndexCount, 0, 0);
	}
}

void Renderer::SortMeshNodes(NodeCollection& nodes, ICamera* camera)
{
	// Reorder the visible meshes for batching
	auto sortStaticMeshCollection = [&camera](vector<SceneNode*>& collection)
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

		sort(collection.begin(), collection.end(), compareDistance);
		stable_sort(collection.begin(), collection.end(), CompareMeshes);
		stable_sort(collection.begin(), collection.end(), CompareMaterials);
		stable_partition(collection.begin(), collection.end(), isOpaque);
	};

	sortStaticMeshCollection(nodes.StaticMeshes);
	sortStaticMeshCollection(nodes.InstancedStaticMeshes);
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

		RenderPass(sceneRoot, camera, RENDER_PASS_TYPE_FORWARD, viewport,
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

	SAFE_RELEASE(bufferBlitVertices);
	SAFE_RELEASE(bufferCameraConstants);
	SAFE_RELEASE(bufferTerrainPatchInstanceConstants);
	SAFE_RELEASE(bufferStaticMeshInstanceConstants);
	SAFE_RELEASE(inputLayoutBlit);
	SAFE_RELEASE(inputLayoutStaticMesh);
	SAFE_RELEASE(inputLayoutStaticMeshInstanced);
	SAFE_RELEASE(inputLayoutTerrainPatch);
	SAFE_RELEASE(samplerStateLinearStaticMesh);
	SAFE_RELEASE(forwardPassRasterState);
	SAFE_RELEASE(wireframeRasterState);
	SAFE_RELEASE(forwardPassDepthStencilState);
	SAFE_RELEASE(blitDepthStencilState);
	SAFE_RELEASE(samplerStateBlit);

	DestroyDeferredTargets();
	DestroyRenderTarget();

	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(deviceContext);
	SAFE_RELEASE(device);
}