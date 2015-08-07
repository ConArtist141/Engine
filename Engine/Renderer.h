#ifndef RENDERER_H_
#define RENDERER_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <vector>

#include "RenderWindow.h"
#include "Geometry.h"

#define STATIC_MESH_VERTEX_ATTRIBUTE_COUNT 7
#define BLIT_ATTRIBUTE_COUNT 2
#define STATIC_MESH_STRIDE 8 * sizeof(float)
#define BLIT_STRIDE 4 * sizeof(float)
#define BLIT_VERTEX_COUNT 4
#define DEFAULT_INSTANCE_CACHE_SIZE 256

#define BLIT_VERTEX_SHADER_LOCATION "CompositeVertex.cso"
#define BLIT_PIXEL_SHADER_LOCATION "CompositePixel.cso"

struct RegionNode;
class SceneNode;
class ICamera;
class BytecodeBlob;
class ContentPackage;

enum RenderPassType
{
	RENDER_PASS_TYPE_DEFERRED = 0,
	RENDER_PASS_TYPE_SHADOW_MAP = 1
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

class Renderer
{
public:
	bool Initialize(HWND hWindow, const RenderParams& params);
	bool Reset(const RenderParams& params);
	void RenderFrame(SceneNode* sceneRoot, ICamera* camera);
	void OnResize();
	void Destroy();

	bool CreateStaticMeshInputLayout(const std::string& vertexShaderFile);
	bool CreateStaticMeshInputLayout(BytecodeBlob& vertexShaderBytecode);

	inline void SetMoveSizeEntered(const bool value);

	inline RenderParams GetRenderParams() const;
	inline bool IsFullscreen() const;
	inline bool IsWindowed() const;
	inline bool MoveSizeEntered() const;
	inline ID3D11Device* GetDevice() const;
	
	const D3D11_INPUT_ELEMENT_DESC* GetStaticMeshInputElementDesc() const;
	const D3D11_INPUT_ELEMENT_DESC* GetBlitInputElementDesc() const;

	Renderer();

protected:
	void CollectVisibleStaticMeshes(RegionNode* node, const Frustum& cameraFrustum, 
		std::vector<SceneNode*>& meshes);
	void CollectVisibleStaticMeshes(SceneNode* node, const Frustum& cameraFrustum, 
		std::vector<SceneNode*>& meshes);

	virtual bool InitWindow(const HWND hWindow, const RenderParams& params);
	virtual bool InitRenderTarget();
	virtual bool InitRenderObjects();
	virtual bool InitDeferredTargets();
	virtual bool InitInternalShaders();
	virtual bool InitInternalVertexBuffers();

	virtual void DeferredPass(SceneNode* sceneRoot, ICamera* camera, const RenderPassType passType,
		D3D11_VIEWPORT& viewport, ID3D11RenderTargetView** renderTargets, const size_t renderTargetsCount,
		ID3D11DepthStencilView* depthStencilView);
	virtual void CompositePass(D3D11_VIEWPORT& viewport);

	virtual void DestroyRenderTarget();
	virtual void DestroyDeferredTargets();

private:
	ResizingCache<DirectX::XMFLOAT4X4> instanceCache;
	bool bMoveSizeEntered;
	bool bDisposed;
	int frameCount;
	RenderParams renderParameters;

	ID3D11InputLayout* staticMeshInputLayout;
	ID3D11InputLayout* blitInputLayout;
	IDXGISwapChain* swapChain;
	ID3D11Device* device;
	ID3D11DeviceContext* deviceContext;
	ID3D11RenderTargetView* backBufferRenderTarget;

	std::vector<ID3D11Texture2D*> deferredBuffers;
	std::vector<ID3D11ShaderResourceView*> deferredShaderResourceViews;
	std::vector<ID3D11ShaderResourceView*> deferredShaderResourceClear;
	std::vector<ID3D11RenderTargetView*> deferredRenderTargets;

	ID3D11Buffer* blitVertexBuffer;
	ID3D11VertexShader* blitVertexShader;
	ID3D11PixelShader* blitPixelShader;

	ID3D11DepthStencilView* defaultDepthStencilView;
	ID3D11Texture2D* depthStencilTexture;
	ID3D11DepthStencilState* forwardPassDepthStencilState;
	ID3D11DepthStencilState* blitDepthStencilState;
	ID3D11RasterizerState* forwardPassRasterState;
	ID3D11SamplerState* linearSamplerState;
	ID3D11SamplerState* blitSamplerState;

	ContentPackage* internalContent;
};

template <typename CacheData>
inline ResizingCache<CacheData>::~ResizingCache()			{ delete[] cache; }

inline RenderParams Renderer::GetRenderParams() const		{ return renderParameters; }
inline bool Renderer::IsFullscreen() const					{ return !renderParameters.Windowed; }
inline bool Renderer::IsWindowed() const					{ return renderParameters.Windowed; }
inline bool Renderer::MoveSizeEntered() const				{ return bMoveSizeEntered; }
inline void Renderer::SetMoveSizeEntered(const bool value)	{ bMoveSizeEntered = value; }
inline ID3D11Device* Renderer::GetDevice() const			{ return device; }

#endif