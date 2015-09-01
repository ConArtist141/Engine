#ifndef CONTENT_PACKAGE_H_
#define CONTENT_PACKAGE_H_

#include <map>
#include <d3d11.h>

#include "InputElementDesc.h"

class StaticMesh;
class Renderer;
class MaterialData;

class BytecodeBlob
{
public:
	void* Bytecode;
	size_t BytecodeLength;

	void Destroy();
};

class ContentPackage
{
protected:
	std::map<std::string, StaticMesh*> staticMeshes;
	std::map<std::string, std::pair<ID3D11Resource*, ID3D11ShaderResourceView*>> textures;
	std::map<std::string, ID3D11VertexShader*> vertexShaders;
	std::map<std::string, ID3D11PixelShader*> pixelShaders;
	std::map<std::string, MaterialData*> materials;

	ID3D11Device* device;

	struct
	{
		int position;
		int texCoord;
		int normal;
		int tangent;
		int bitangent;
	} offsets;

	size_t vertexStrideFloat;
	size_t vertexStrideByte;

public:
	ContentPackage(ID3D11Device* device);
	ContentPackage(Renderer* renderer);

	void SetVertexLayout(const InputElementLayout* layout);

	bool LoadMesh(const std::string& contentLocation, StaticMesh** meshOut);
	bool LoadTexture2D(const std::string& contentLocation, ID3D11Resource** texture,
		ID3D11ShaderResourceView** resourceView);

	// Creates a vertex shader from a file and outputs a bytecode blob. The vertex shader
	// is owned by the content package, but the caller owns the bytecode blob.
	bool LoadVertexShader(const std::string& contentLocation, ID3D11VertexShader** shaderOut,
		BytecodeBlob* bytecodeOut);

	bool LoadVertexShader(const std::string& contentLocation, ID3D11VertexShader** shaderOut);
	bool LoadPixelShader(const std::string& contentLocation, ID3D11PixelShader** shaderOut);

	void SetMaterial(const std::string& contentName, MaterialData* material);
	bool GetMaterial(const std::string& contentName, MaterialData** material);

	void Destroy();
};

#endif