#include "ContentPackage.h"

#include "StaticMesh.h"
#include "Renderer.h"
#include "DDSTextureLoader.h"
#include "MaterialData.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stdint.h>
#include <string.h>
#include <vector>
#include <fstream>
#include <sstream>

using namespace std;
using namespace DirectX;

void BytecodeBlob::Destroy()
{
	delete[] reinterpret_cast<char*>(Bytecode);
}

ContentPackage::ContentPackage(ID3D11Device* device) :
device(device)
{
}

ContentPackage::ContentPackage(Renderer* renderer) :
device(renderer->GetDevice())
{
}

void ContentPackage::SetVertexLayout(const InputElementLayout* layout)
{
	vertexStrideByte = layout->Stride;
	vertexStrideFloat = layout->Stride / sizeof(float);

	offsets.position = VERTEX_ATTRIBUTE_DISABLED;
	offsets.texCoord = VERTEX_ATTRIBUTE_DISABLED;
	offsets.normal = VERTEX_ATTRIBUTE_DISABLED;
	offsets.tangent = VERTEX_ATTRIBUTE_DISABLED;
	offsets.bitangent = VERTEX_ATTRIBUTE_DISABLED;

	for (size_t attribId = 0; attribId < layout->AttributeCount; ++attribId)
	{
		const D3D11_INPUT_ELEMENT_DESC* desc = &layout->Desc[attribId];
		if (desc->InputSlot != 0)
			continue;

		if (strcmp(desc->SemanticName, "POSITION") == 0)
		{
			if (desc->Format != DXGI_FORMAT_R32G32B32_FLOAT)
				OutputDebugString("Warning: ContentPackage position format not DXGI_FORMAT_R32G32B32_FLOAT\n");
			offsets.position = desc->AlignedByteOffset / sizeof(float);
		}
		else if (strcmp(desc->SemanticName, "TEXCOORD") == 0)
		{
			if (desc->Format != DXGI_FORMAT_R32G32_FLOAT)
				OutputDebugString("Warning: ContentPackage tex coord format not DXGI_FORMAT_R32G32_FLOAT\n");
			offsets.texCoord = desc->AlignedByteOffset / sizeof(float);
		}
		else if (strcmp(desc->SemanticName, "NORMAL") == 0)
		{
			if (desc->Format != DXGI_FORMAT_R32G32B32_FLOAT)
				OutputDebugString("Warning: ContentPackage normal format not DXGI_FORMAT_R32G32B32_FLOAT\n");
			offsets.normal = desc->AlignedByteOffset / sizeof(float);
		}
		else if (strcmp(desc->SemanticName, "TANGENT") == 0)
		{
			if (desc->Format != DXGI_FORMAT_R32G32B32_FLOAT)
				OutputDebugString("Warning: ContentPackage tangent format not DXGI_FORMAT_R32G32B32_FLOAT\n");
			offsets.tangent = desc->AlignedByteOffset / sizeof(float);
		}
		else if (strcmp(desc->SemanticName, "BITANGENT") == 0)
		{
			if (desc->Format != DXGI_FORMAT_R32G32B32_FLOAT)
				OutputDebugString("Warning: ContentPackage bitangent format not DXGI_FORMAT_R32G32B32_FLOAT\n");
			offsets.bitangent = desc->AlignedByteOffset / sizeof(float);
		}
	}
}

bool ContentPackage::LoadMesh(const std::string& contentLocation, StaticMesh** meshOut)
{
	OutputDebugString("Loading resource ");
	OutputDebugString(contentLocation.c_str());
	OutputDebugString("...\n");

	if (vertexStrideFloat == 0)
	{
		OutputDebugString("Vertex layout has not been set!\n");
		return false;
	}

	auto findResult = staticMeshes.find(contentLocation);
	if (findResult != staticMeshes.end())
	{
		*meshOut = findResult->second;
		return true;
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(contentLocation.c_str(),
		(aiProcessPreset_TargetRealtime_Fast |
		aiProcess_FlipUVs |
		aiProcess_PreTransformVertices));

	if (scene == nullptr)
		return false;

	float* meshData = nullptr;

	if (!scene->HasMeshes())
	{
		OutputDebugString("Scene does not have meshes!\n");
		return false;
	}

	size_t dataSize = 0;
	for (size_t i = 0; i < scene->mNumMeshes; ++i)
	{
		auto mesh = scene->mMeshes[i];
		if (!mesh->HasPositions() && (offsets.position != VERTEX_ATTRIBUTE_DISABLED))
		{
			OutputDebugString("Mesh is missing positions!\n");
			return false;
		}

		if (!mesh->HasTextureCoords(0) && (offsets.texCoord != VERTEX_ATTRIBUTE_DISABLED))
		{
			OutputDebugString("Mesh is missing texture coordinates at location 0!\n");
			return false;
		}

		if (!mesh->HasNormals() && (offsets.normal != VERTEX_ATTRIBUTE_DISABLED))
		{
			OutputDebugString("Mesh is missing normals!\n");
			return false;
		}

		if (!mesh->HasTangentsAndBitangents() && ((offsets.tangent != VERTEX_ATTRIBUTE_DISABLED) ||
			(offsets.bitangent != VERTEX_ATTRIBUTE_DISABLED)))
		{
			OutputDebugString("Mesh is missing tangets or bitangents!\n");
			return false;
		}

		dataSize += mesh->mNumVertices;
	}

	dataSize *= vertexStrideFloat;
	meshData = new float[dataSize];

	DXGI_FORMAT indexFormat;
	if (dataSize <= UINT16_MAX)
		indexFormat = DXGI_FORMAT_R16_UINT;
	else
		indexFormat = DXGI_FORMAT_R32_UINT;

	size_t meshOffset = 0;
	for (size_t i = 0; i < scene->mNumMeshes; ++i)
	{
		auto mesh = scene->mMeshes[i];
		size_t meshDataSize = vertexStrideFloat * mesh->mNumVertices;
		if (offsets.position != VERTEX_ATTRIBUTE_DISABLED)
		{
			for (size_t dataLoc = meshOffset + offsets.position, vertexId = offsets.position, 
				end = meshOffset + meshDataSize;
				dataLoc < end;
				dataLoc += vertexStrideFloat, ++vertexId)
			{
				meshData[dataLoc] = mesh->mVertices[vertexId].x;
				meshData[dataLoc + 1] = mesh->mVertices[vertexId].y;
				meshData[dataLoc + 2] = mesh->mVertices[vertexId].z;
			}
		}

		if (offsets.texCoord != VERTEX_ATTRIBUTE_DISABLED)
		{
			for (size_t dataLoc = meshOffset + offsets.texCoord, vertexId = 0, 
				end = meshOffset + meshDataSize;
				dataLoc < end;
				dataLoc += vertexStrideFloat, ++vertexId)
			{
				meshData[dataLoc] = mesh->mTextureCoords[0][vertexId].x;
				meshData[dataLoc + 1] = mesh->mTextureCoords[0][vertexId].y;
			}
		}

		if (offsets.normal != VERTEX_ATTRIBUTE_DISABLED)
		{
			for (size_t dataLoc = meshOffset + offsets.normal, vertexId = 0, 
				end = meshOffset + meshDataSize;
				dataLoc < end;
				dataLoc += vertexStrideFloat, ++vertexId)
			{
				meshData[dataLoc] = mesh->mNormals[vertexId].x;
				meshData[dataLoc + 1] = mesh->mNormals[vertexId].y;
				meshData[dataLoc + 2] = mesh->mNormals[vertexId].z;
			}
		}

		if (offsets.tangent != VERTEX_ATTRIBUTE_DISABLED)
		{
			for (size_t dataLoc = meshOffset + offsets.tangent, vertexId = 0, 
				end = meshOffset + meshDataSize;
				dataLoc < end;
				dataLoc += vertexStrideFloat, ++vertexId)
			{
				meshData[dataLoc] = mesh->mTangents[vertexId].x;
				meshData[dataLoc + 1] = mesh->mTangents[vertexId].y;
				meshData[dataLoc + 2] = mesh->mTangents[vertexId].z;
			}
		}

		if (offsets.bitangent != VERTEX_ATTRIBUTE_DISABLED)
		{
			for (size_t dataLoc = meshOffset + offsets.bitangent, vertexId = 0, 
				end = meshOffset + meshDataSize;
				dataLoc < end;
				dataLoc += vertexStrideFloat, ++vertexId)
			{
				meshData[dataLoc] = mesh->mBitangents[vertexId].x;
				meshData[dataLoc + 1] = mesh->mBitangents[vertexId].y;
				meshData[dataLoc + 2] = mesh->mBitangents[vertexId].z;
			}
		}

		meshOffset += mesh->mNumVertices * vertexStrideFloat;
	}

	size_t indexCount = 0;
	for (size_t i = 0; i < scene->mNumMeshes; ++i)
		indexCount += scene->mMeshes[i]->mNumFaces * 3;

	void* meshIndices = nullptr;
	if (indexFormat == DXGI_FORMAT_R16_UINT)
	{
		uint16_t* meshIndices16 = new uint16_t[indexCount];
		meshIndices = meshIndices16;

		size_t indexOffset = 0;
		meshOffset = 0;
		for (size_t i = 0; i < scene->mNumMeshes; ++i)
		{
			auto mesh = scene->mMeshes[i];
			for (size_t faceId = 0, indexId = indexOffset; faceId < mesh->mNumFaces; ++faceId)
			{
				meshIndices16[indexId++] = mesh->mFaces[faceId].mIndices[0] + static_cast<uint16_t>(meshOffset);
				meshIndices16[indexId++] = mesh->mFaces[faceId].mIndices[1] + static_cast<uint16_t>(meshOffset);
				meshIndices16[indexId++] = mesh->mFaces[faceId].mIndices[2] + static_cast<uint16_t>(meshOffset);
			}
			indexOffset += mesh->mNumFaces * 3;
			meshOffset += mesh->mNumVertices * vertexStrideFloat;
		}
	}
	else
	{
		uint32_t* meshIndices32 = new uint32_t[indexCount];
		meshIndices = meshIndices32;

		size_t indexOffset = 0;
		meshOffset = 0;
		for (size_t i = 0; i < scene->mNumMeshes; ++i)
		{
			auto mesh = scene->mMeshes[i];
			for (size_t faceId = 0, indexId = indexOffset; faceId < mesh->mNumFaces; ++faceId)
			{
				meshIndices32[indexId++] = mesh->mFaces[faceId].mIndices[0] + meshOffset;
				meshIndices32[indexId++] = mesh->mFaces[faceId].mIndices[1] + meshOffset;
				meshIndices32[indexId++] = mesh->mFaces[faceId].mIndices[2] + meshOffset;
			}
			indexOffset += mesh->mNumFaces * 3;
			meshOffset += mesh->mNumVertices * vertexStrideFloat;
		}
	}

	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = sizeof(float) * dataSize;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;

	D3D11_SUBRESOURCE_DATA bufferData;
	ZeroMemory(&bufferData, sizeof(bufferData));
	bufferData.pSysMem = meshData;

	ID3D11Buffer* vertexBuffer;
	HRESULT result = device->CreateBuffer(&bufferDesc, &bufferData, &vertexBuffer);
	if (FAILED(result))
	{
		delete[] meshData;
		delete[] meshIndices;
		return false;
	}

	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	if (indexFormat == DXGI_FORMAT_R16_UINT)
		bufferDesc.ByteWidth = indexCount * sizeof(uint16_t);
	else
		bufferDesc.ByteWidth = indexCount * sizeof(uint32_t);
	bufferData.pSysMem = meshIndices;

	ID3D11Buffer* indexBuffer;
	result = device->CreateBuffer(&bufferDesc, &bufferData, &indexBuffer);

	delete[] meshData;
	delete[] meshIndices;

	if (FAILED(result))
	{
		vertexBuffer->Release();
		return false;
	}

	auto infinity = numeric_limits<float>::infinity();
	Bounds bounds = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };

	for (size_t i = 0; i < scene->mNumMeshes; ++i)
	{
		auto mesh = scene->mMeshes[i];
		for (size_t vertexId = 0; vertexId < mesh->mNumVertices; ++vertexId)
		{
			auto vertex = mesh->mVertices[vertexId];
			if (vertex.x > bounds.Upper.x)
				bounds.Upper.x = vertex.x;
			if (vertex.y > bounds.Upper.y)
				bounds.Upper.y = vertex.y;
			if (vertex.z > bounds.Upper.z)
				bounds.Upper.z = vertex.z;

			if (vertex.x < bounds.Lower.x)
				bounds.Lower.x = vertex.x;
			if (vertex.y < bounds.Lower.y)
				bounds.Lower.y = vertex.y;
			if (vertex.z < bounds.Lower.z)
				bounds.Lower.z = vertex.z;
		}
	}

	stringstream strstream;
	strstream << "Computed Mesh Bounds : { (" << bounds.Lower.x << ", " << bounds.Lower.y << ", " <<
		bounds.Lower.z << "), (" << bounds.Upper.x << ", " << bounds.Upper.y << ", " << bounds.Upper.z <<
		") }\n";
	OutputDebugString(strstream.str().c_str());

	*meshOut = new StaticMesh(vertexBuffer, indexBuffer, indexCount, 0, bounds, indexFormat);
	staticMeshes[contentLocation] = *meshOut;
	return true;
}

bool ContentPackage::LoadTexture2D(const std::string& contentLocation, ID3D11Resource** textureResource,
	ID3D11ShaderResourceView** resourceView)
{
	OutputDebugString("Loading resource ");
	OutputDebugString(contentLocation.c_str());
	OutputDebugString("...\n");

	auto findResult = textures.find(contentLocation);
	if (findResult != textures.end())
	{
		*textureResource = findResult->second.first;
		*resourceView = findResult->second.second;
		return true;
	}

	std::ifstream textureStream(contentLocation, std::ifstream::in | std::ifstream::binary);

	if (textureStream.fail())
		return false;

	textureStream.seekg(0, std::ios::end);
	auto size = textureStream.tellg();
	textureStream.seekg(0, std::ios::beg);

	char* data = new char[static_cast<size_t>(size)];
	textureStream.read(data, size);
	textureStream.close();

	HRESULT result = CreateDDSTextureFromMemory(device, reinterpret_cast<uint8_t*>(data),
		static_cast<size_t>(size), textureResource, resourceView);

	delete[] data;
	if (FAILED(result))
		return false;

	textures[contentLocation] = make_pair(*textureResource, *resourceView);

	return true;
}

bool ContentPackage::LoadVertexShader(const std::string& contentLocation, ID3D11VertexShader** shaderOut)
{
	return LoadVertexShader(contentLocation, shaderOut, nullptr);
}

bool ContentPackage::LoadVertexShader(const std::string& contentLocation, ID3D11VertexShader** shaderOut,
	BytecodeBlob* bytecodeOut)
{
	OutputDebugString("Loading resource ");
	OutputDebugString(contentLocation.c_str());
	OutputDebugString("...\n");

	auto findResult = vertexShaders.find(contentLocation);
	if (findResult != vertexShaders.end())
	{
		*shaderOut = findResult->second;

		if (bytecodeOut != nullptr)
			bytecodeOut->Bytecode = nullptr;

		return true;
	}

	char* bytecode = nullptr;
	size_t bytecodeLength;
	std::ifstream t(contentLocation, std::ios::binary | std::ios::in);
	if (t.fail())
		return false;

	t.seekg(0, std::ios::end);
	bytecodeLength = static_cast<size_t>(t.tellg());
	t.seekg(0, std::ios::beg);

	bytecode = new char[bytecodeLength];
	t.read(bytecode, bytecodeLength);
	t.close();

	HRESULT result = device->CreateVertexShader(bytecode, bytecodeLength, nullptr, shaderOut);

	if (bytecodeOut != nullptr)
	{
		bytecodeOut->Bytecode = bytecode;
		bytecodeOut->BytecodeLength = bytecodeLength;
	}
	else
		delete[] bytecode;

	if (FAILED(result))
		return false;

	vertexShaders[contentLocation] = *shaderOut;

	return true;
}

bool ContentPackage::LoadPixelShader(const std::string& contentLocation, ID3D11PixelShader** shaderOut)
{
	OutputDebugString("Loading resource ");
	OutputDebugString(contentLocation.c_str());
	OutputDebugString("...\n");

	auto findResult = pixelShaders.find(contentLocation);
	if (findResult != pixelShaders.end())
	{
		*shaderOut = findResult->second;
		return true;
	}

	char* bytecode = nullptr;
	size_t bytecodeLength;
	std::ifstream t(contentLocation, std::ios::binary | std::ios::in);
	if (t.fail())
		return false;

	t.seekg(0, std::ios::end);
	bytecodeLength = static_cast<size_t>(t.tellg());
	t.seekg(0, std::ios::beg);

	bytecode = new char[bytecodeLength];
	t.read(bytecode, bytecodeLength);
	t.close();

	HRESULT result = device->CreatePixelShader(bytecode, bytecodeLength, nullptr, shaderOut);

	delete[] bytecode;

	if (FAILED(result))
		return false;

	pixelShaders[contentLocation] = *shaderOut;

	return true;
}

void ContentPackage::SetMaterial(const std::string& contentName, MaterialData* material)
{
#ifdef _DEBUG
	auto it = materials.find(contentName);
	if (it != materials.end())
		OutputDebugString("Warning: Conflicting material found!\n");
#endif

	materials[contentName] = material;
}

bool ContentPackage::GetMaterial(const std::string& contentName, MaterialData** material)
{
	auto it = materials.find(contentName);
	if (it != materials.end())
		*material = it->second;
	return false;
}

void ContentPackage::Destroy()
{
	for (auto material : materials)
	{
		material.second->Destroy();
		delete material.second;

		OutputDebugString("Destroying material ");
		OutputDebugString(material.first.c_str());
		OutputDebugString("....\n");
	}

	for (auto mesh : staticMeshes)
	{
		mesh.second->Destroy();
		delete mesh.second;

		OutputDebugString("Destroying resource ");
		OutputDebugString(mesh.first.c_str());
		OutputDebugString("....\n");
	}

	for (auto texture : textures)
	{
		texture.second.second->Release();
		texture.second.first->Release();

		OutputDebugString("Destroying resource ");
		OutputDebugString(texture.first.c_str());
		OutputDebugString("....\n");
	}

	for (auto shader : pixelShaders)
	{
		shader.second->Release();

		OutputDebugString("Destroying resource ");
		OutputDebugString(shader.first.c_str());
		OutputDebugString("....\n");
	}

	for (auto shader : vertexShaders)
	{
		shader.second->Release();

		OutputDebugString("Destroying resource ");
		OutputDebugString(shader.first.c_str());
		OutputDebugString("....\n");
	}
}