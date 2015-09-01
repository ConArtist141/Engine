#ifndef RENDERER_H_
#define RENDERER_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <vector>

#include "RenderWindow.h"
#include "Geometry.h"
#include "InputElementDesc.h"

#define BLIT_VERTEX_COUNT 4
#define DEFAULT_INSTANCE_CACHE_SIZE 256

#define STATIC_MESH_INSTANCED_VERTEX_SHADER_LOCATION "StaticMeshInstancedVertex.cso"
#define STATIC_MESH_INSTANCED_PIXEL_SHADER_LOCATION "StaticMeshInstancedPixel.cso"
#define TERRAIN_PATCH_VERTEX_SHADER_LOCATION "TerrainPatchVertex.cso"
#define TERRAIN_PATCH_PIXEL_SHADER_LOCATION "TerrainPatchPixel.cso"
#define BLIT_VERTEX_SHADER_LOCATION "CompositeVertex.cso"
#define BLIT_PIXEL_SHADER_LOCATION "CompositePixel.cso"

struct RegionNode;
class SceneNode;
class ICamera;
class BytecodeBlob;
class ContentPackage;

enum RenderPassType
{
	RENDER_PASS_TYPE_FORWARD,
	RENDER_PASS_TYPE_DEFERRED,
	RENDER_PASS_TYPE_SHADOW_MAP
};

enum RenderTargetIndex
{
	RENDER_TARGET_INDEX_ALBEDO = 0
};

template <typename CacheData>
class ResizingCache
{
private:
	CacheData* cache;
	size_t cacheReserved;
	size_t cacheSize;

public:
	ResizingCache(const size_t reserve);
	inline ~ResizingCache();

	inline size_t GetSize() const;
	inline size_t GetReservedSize() const;
	inline CacheData* GetData() const;
	void Reserve(const size_t reserveCount);
	inline void Push(const CacheData& data);
	inline void Clear();
};

struct NodeCollection
{
	std::vector<SceneNode*> InstancedStaticMeshes;
	std::vector<SceneNode*> TerrainPatches;
};

class Renderer
{
public:
	bool Initialize(HWND hWindow, const RenderParams& params);
	bool Reset(const RenderParams& params);
	void RenderFrame(SceneNode* sceneRoot, ICamera* camera);
	void OnResize();
	void Destroy();

	inline void SetMoveSizeEntered(const bool value);

	inline RenderParams GetRenderParams() const;
	inline bool IsFullscreen() const;
	inline bool IsWindowed() const;
	inline bool MoveSizeEntered() const;
	inline ID3D11Device* GetDevice() const;

	inline const InputElementLayout* GetElementLayoutStaticMesh() const;
	inline const InputElementLayout* GetElementLayoutStaticMeshInstanced() const;
	inline const InputElementLayout* GetElementLayoutBlit() const;
	inline const InputElementLayout* GetElementLayoutTerrainPatch() const;

	Renderer();

protected:
	void CollectVisibleNodes(RegionNode* node, const Frustum& cameraFrustum, NodeCollection& nodes);
	void CollectVisibleNodes(SceneNode* node, const Frustum& cameraFrustum, NodeCollection& nodes);

	virtual bool InitWindow(const HWND hWindow, const RenderParams& params);
	virtual bool InitRenderTarget();
	virtual bool InitRenderObjects();
	virtual bool InitInternalShaders();
	virtual bool InitInternalVertexBuffers();

	virtual void RenderPass(SceneNode* sceneRoot, ICamera* camera, const RenderPassType passType,
		D3D11_VIEWPORT& viewport, ID3D11RenderTargetView** renderTargets, const size_t renderTargetsCount,
		ID3D11DepthStencilView* depthStencilView);

	virtual void RenderStaticMeshesInstanced(SceneNode* sceneRoot, ICamera* camera, NodeCollection& nodes);
	virtual void RenderTerrainPatches(SceneNode* sceneRoot, ICamera* camera, NodeCollection& nodes);

	virtual void DestroyRenderTarget();
	virtual void DestroyDeferredTargets();

private:
	ResizingCache<DirectX::XMFLOAT4X4> instanceCache;
	bool bMoveSizeEntered;
	bool bDisposed;
	int frameCount;
	RenderParams renderParameters;

	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
	ID3D11RenderTargetView* backBufferRenderTarget;

	std::vector<ID3D11Texture2D*> deferredBuffers;
	std::vector<ID3D11ShaderResourceView*> deferredShaderResourceViews;
	std::vector<ID3D11ShaderResourceView*> deferredShaderResourceClear;
	std::vector<ID3D11RenderTargetView*> deferredRenderTargets;

	ID3D11Buffer* vertexBufferBlit;

	ID3D11VertexShader* vertexShaderBlit;
	ID3D11VertexShader* vertexShaderStaticMeshInstanced;
	ID3D11VertexShader* vertexShaderTerrainPatch;
	ID3D11PixelShader* pixelShaderBlit;
	ID3D11PixelShader* pixelShaderStaticMeshInstanced;
	ID3D11PixelShader* pixelShaderTerrainPatch;

	ID3D11DepthStencilView* defaultDepthStencilView;
	ID3D11Texture2D* depthStencilTexture;
	ID3D11DepthStencilState* forwardPassDepthStencilState;
	ID3D11DepthStencilState* blitDepthStencilState;
	ID3D11RasterizerState* forwardPassRasterState;
	ID3D11SamplerState* linearSamplerState;
	ID3D11SamplerState* blitSamplerState;

	InputElementLayout elementLayoutStaticMesh;
	InputElementLayout elementLayoutStaticMeshInstanced;
	InputElementLayout elementLayoutBlit;
	InputElementLayout elementLayoutTerrainPatch;

	ID3D11InputLayout* inputLayoutStaticMeshInstanced;
	ID3D11InputLayout* inputLayoutBlit;
	ID3D11InputLayout* inputLayoutTerrainPatch;

	ContentPackage* internalContent;
};

template <typename CacheData>
inline ResizingCache<CacheData>::~ResizingCache()			
{ delete[] cache; }
inline RenderParams Renderer::GetRenderParams() const		
{ return renderParameters; }
inline bool Renderer::IsFullscreen() const					
{ return !renderParameters.Windowed; }
inline bool Renderer::IsWindowed() const					
{ return renderParameters.Windowed; }
inline bool Renderer::MoveSizeEntered() const				
{ return bMoveSizeEntered; }
inline void Renderer::SetMoveSizeEntered(const bool value)	
{ bMoveSizeEntered = value; }
inline ID3D11Device* Renderer::GetDevice() const			
{ return device; }
inline const InputElementLayout * Renderer::GetElementLayoutStaticMesh() const
{ return &elementLayoutStaticMesh; }
inline const InputElementLayout * Renderer::GetElementLayoutStaticMeshInstanced() const
{ return &elementLayoutStaticMeshInstanced; }
inline const InputElementLayout * Renderer::GetElementLayoutBlit() const
{ return &elementLayoutBlit; }
inline const InputElementLayout * Renderer::GetElementLayoutTerrainPatch() const
{ return &elementLayoutTerrainPatch; }

#endif