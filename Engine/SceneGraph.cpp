#include "SceneGraph.h"

#include <stack>
#include <limits>

using namespace DirectX;
using namespace std;

void GetNodeBoundsZone(const SceneNode* node, Bounds* boundsOut)
{
	*boundsOut = node->Region.AABB;
}

void GetNodeBoundsStaticMesh(const SceneNode* node, Bounds* boundsOut)
{
	node->Ref.StaticMesh->GetMeshBounds(boundsOut);
	XMMATRIX matrix = XMLoadFloat4x4(&node->Transform.Global);
	TransformBounds(matrix, *boundsOut, boundsOut);
}

void GetNodeBoundsTerrainPatch(const SceneNode* node, Bounds* boundsOut)
{
	node->Ref.TerrainPatch->GetBounds(boundsOut);
	XMMATRIX matrix = XMLoadFloat4x4(&node->Transform.Global);
	TransformBounds(matrix, *boundsOut, boundsOut);
}

void GetNodeBoundsEndMarker(const SceneNode* node, Bounds* boundsOut)
{
}

SceneNodeFunctionTableEntry SceneNodeFunctionTable[NODE_TYPE_END_ENUM + 1] =
{
	{ nullptr }, // NODE_TYPE_EMPTY
	{ &GetNodeBoundsZone }, // NODE_TYPE_ZONE
	{ &GetNodeBoundsStaticMesh }, // NODE_TYPE_STATIC_MESH
	{ &GetNodeBoundsStaticMesh }, // NODE_TYPE_STATIC_MESH_INSTANCED
	{ &GetNodeBoundsTerrainPatch }, // NODE_TYPE_TERRAIN_PATCH
	{ nullptr }, // NODE_TYPE_LIGHT

	{ &GetNodeBoundsEndMarker } // End marker
};

#ifdef _DEBUG
struct DebugFunctionTableCheck
{
	DebugFunctionTableCheck()
	{
		assert(SceneNodeFunctionTable[NODE_TYPE_END_ENUM].GetNodeBounds == &GetNodeBoundsEndMarker);
	}
};
DebugFunctionTableCheck FunctionTableCheck;
#endif

void UpdateTransforms(SceneNode* node, const XMMATRIX& transform)
{
	XMMATRIX matrix = XMLoadFloat4x4(&node->Transform.Local);
	matrix = XMMatrixMultiply(matrix, transform);
	XMStoreFloat4x4(&node->Transform.Global, matrix);

	for (auto child : node->Children)
		UpdateTransforms(child, matrix);
}

void CollectZoneLeaves(SceneNode* node, std::vector<SceneNode*>* leaves, 
	std::vector<SceneNode*>* omniLights, SceneNode** directionalLight)
{
	directionalLight = nullptr;

	for (auto child : node->Children)
	{
		// Collect static meshes and zones.
		if (child->IsMesh())
			leaves->push_back(child);
		else if (child->IsZone())
			leaves->push_back(child);
		else if (child->IsLight())
		{
			switch (child->Ref.LightData->Type)
			{
			case LIGHT_TYPE_DIRECTIONAL:
				*directionalLight = child;
				break;

			case LIGHT_TYPE_OMNI:
				omniLights->push_back(child);
				break;
			}
		}
	}
}

void GetVolumeLeafBounds(SceneNode* node, Bounds* boundsOut)
{
	auto getBoundsFunc = SceneNodeFunctionTable[node->Type].GetNodeBounds;

	if (getBoundsFunc != nullptr)
		getBoundsFunc(node, boundsOut);
	else
	{
		OutputDebugString("Invalid leaf type!\n");
		assert(1);
	}
}

void CreateHierarchyFromBlob(vector<RegionNode*> regions, RegionNode* baseRegion)
{
	// Compute bounding box of the blob
	auto infinity = numeric_limits<float>::infinity();
	Bounds bounds = { { infinity, infinity, infinity }, { -infinity, -infinity, -infinity } };
	for (auto region : regions)
	{
		if (bounds.Lower.x > region->AABB.Lower.x)
			bounds.Lower.x = region->AABB.Lower.x;
		if (bounds.Lower.y > region->AABB.Lower.y)
			bounds.Lower.y = region->AABB.Lower.y;
		if (bounds.Lower.z > region->AABB.Lower.z)
			bounds.Lower.z = region->AABB.Lower.z;

		if (bounds.Upper.x < region->AABB.Upper.x)
			bounds.Upper.x = region->AABB.Upper.x;
		if (bounds.Upper.y < region->AABB.Upper.y)
			bounds.Upper.y = region->AABB.Upper.y;
		if (bounds.Upper.z < region->AABB.Upper.z)
			bounds.Upper.z = region->AABB.Upper.z;
	}

	baseRegion->AABB = bounds;

	// Find longest axis of the blob
	auto widthX = bounds.Upper.x - bounds.Lower.x;
	auto widthY = bounds.Upper.y - bounds.Lower.y;
	auto widthZ = bounds.Upper.z - bounds.Lower.z;
	auto maxWidth = std::fmax(std::fmax(widthX, widthY), widthZ);
	auto majorAxis = MAJOR_AXIS_X;
	if (widthY == maxWidth)
		majorAxis = MAJOR_AXIS_Y;
	else if (widthZ == maxWidth)
		majorAxis = MAJOR_AXIS_Z;

	// Split the blob into three blobs
	vector<RegionNode*> lesserBlob;
	vector<RegionNode*> centerBlob;
	vector<RegionNode*> greaterBlob;

	switch (majorAxis)
	{
	case MAJOR_AXIS_X:
	{
		auto centerX = 0.5f * (bounds.Upper.x + bounds.Lower.x);
		for (auto region : regions)
		{
			if (region->AABB.Upper.x < centerX)
				lesserBlob.push_back(region);
			else if (region->AABB.Lower.x > centerX)
				greaterBlob.push_back(region);
			else
				centerBlob.push_back(region);
		}

		break;
	}
	case MAJOR_AXIS_Y:
	{
		auto centerY = 0.5f * (bounds.Upper.y + bounds.Lower.y);
		for (auto region : regions)
		{
			if (region->AABB.Upper.y < centerY)
				lesserBlob.push_back(region);
			else if (region->AABB.Lower.y > centerY)
				greaterBlob.push_back(region);
			else
				centerBlob.push_back(region);
		}

		break;
	}
	case MAJOR_AXIS_Z:
	{
		auto centerZ = 0.5f * (bounds.Upper.z + bounds.Lower.z);
		for (auto region : regions)
		{
			if (region->AABB.Upper.z < centerZ)
				lesserBlob.push_back(region);
			else if (region->AABB.Lower.z > centerZ)
				greaterBlob.push_back(region);
			else
				centerBlob.push_back(region);
		}

		break;
	}
	}

	// We couldn't split the blob, split the blob arbitrarily.
	// The only case where the blob cannot be split is where they all intersect the center plane
	// and are placed in the sub-center blob.
	if (centerBlob.size() == regions.size())
	{
		centerBlob.clear();

		vector<RegionNode*>* blobBuckets[] = { &lesserBlob, &centerBlob, &greaterBlob };
		size_t index = 0;
		for (auto region : regions)
		{
			blobBuckets[index % 3]->push_back(region);
			++index;
		}
	}

	// Handle blob 1
	if (lesserBlob.size() == 0)
		baseRegion->Node1 = nullptr;
	else if (lesserBlob.size() == 1)
		baseRegion->Node1 = lesserBlob[0];
	else
	{
		baseRegion->Node1 = new RegionNode;
		baseRegion->Node1->LeafData = nullptr;
		CreateHierarchyFromBlob(lesserBlob, baseRegion->Node1);
	}

	// Handle blob 2
	if (centerBlob.size() == 0)
		baseRegion->Node2 = nullptr;
	else if (centerBlob.size() == 1)
		baseRegion->Node2 = centerBlob[0];
	else
	{
		baseRegion->Node2 = new RegionNode;
		baseRegion->Node2->LeafData = nullptr;
		CreateHierarchyFromBlob(centerBlob, baseRegion->Node2);
	}

	// Handle blob 3
	if (greaterBlob.size() == 0)
		baseRegion->Node3 = nullptr;
	else if (greaterBlob.size() == 1)
		baseRegion->Node3 = greaterBlob[0];
	else
	{
		baseRegion->Node3 = new RegionNode;
		baseRegion->Node3->LeafData = nullptr;
		CreateHierarchyFromBlob(greaterBlob, baseRegion->Node3);
	}
}

void DestroyHierarchyRegion(RegionNode* node, const bool bDestroyChildrenHierarchies)
{
	if (bDestroyChildrenHierarchies && node->LeafData != nullptr && node->LeafData->IsZone())
		DestroySceneGraphHierarchy(node->LeafData, true);
	else
	{
		if (node->Node1 != nullptr)
			DestroyHierarchyRegion(node->Node1, bDestroyChildrenHierarchies);
		if (node->Node2 != nullptr)
			DestroyHierarchyRegion(node->Node2, bDestroyChildrenHierarchies);
		if (node->Node3 != nullptr)
			DestroyHierarchyRegion(node->Node3, bDestroyChildrenHierarchies);
	}

	delete node;
}

void DestroySceneGraphHierarchy(SceneNode* zone, const bool bDestroyChildrenHierarchies)
{
	if (zone->IsZone())
	{
		if (zone->Region.Node1 != nullptr)
		{
			DestroyHierarchyRegion(zone->Region.Node1, bDestroyChildrenHierarchies);
			zone->Region.Node1 = nullptr;
		}

		if (zone->Region.Node2 != nullptr)
		{
			DestroyHierarchyRegion(zone->Region.Node2, bDestroyChildrenHierarchies);
			zone->Region.Node2 = nullptr;
		}

		if (zone->Region.Node3 != nullptr)
		{
			DestroyHierarchyRegion(zone->Region.Node3, bDestroyChildrenHierarchies);
			zone->Region.Node3 = nullptr;
		}
	}
}

void BuildSceneGraphHierarchy(SceneNode* zone, const bool bRebuildChildrenZones)
{
	if (!zone->IsZone())
	{
		OutputDebugString("Scene node specified is not a zone!\n");
		return;
	}

	DestroySceneGraphHierarchy(zone, bRebuildChildrenZones);

	ZoneData* zoneData = zone->Ref.ZoneData;
	vector<SceneNode*> leaves;
	CollectZoneLeaves(zone, &leaves, &zoneData->OmniLights, &zoneData->DirectionalLight);

	vector<RegionNode*> leafRegions;
	for (auto leaf : leaves)
	{
		Bounds bounds;

		// Rebuild children
		if (bRebuildChildrenZones && leaf->IsZone())
			BuildSceneGraphHierarchy(leaf, true);

		// Update region
		GetVolumeLeafBounds(leaf, &bounds);
		leaf->Region.AABB = bounds;

		auto region = new RegionNode{ bounds, nullptr, nullptr, nullptr, leaf };
		leafRegions.push_back(region);
	}

	CreateHierarchyFromBlob(leafRegions, &zone->Region);
}

SceneNode* CreateSceneGraph()
{
	auto infinity = numeric_limits<float>::infinity();
	auto node = new SceneNode;
	node->Region.AABB =
	{
		{ -infinity, -infinity, -infinity },
		{ infinity, infinity, infinity }
	};
	node->Region.LeafData = nullptr;
	node->Region.Node1 = nullptr;
	node->Region.Node2 = nullptr;
	node->Region.Node3 = nullptr;
	node->Ref.ZoneData = nullptr;
	node->Type = NODE_TYPE_ZONE;

	auto identity = XMMatrixIdentity();
	XMStoreFloat4x4(&node->Transform.Local, identity);

	return node;
}

void DestroySceneGraph(SceneNode* sceneNode)
{
	if (sceneNode->IsZone())
		DestroySceneGraphHierarchy(sceneNode, true);

	for (auto child : sceneNode->Children)
		DestroySceneGraph(child);

	delete sceneNode;
}

SceneNode* CreateStaticMeshNode(StaticMesh* mesh, MaterialData* material, const XMFLOAT4X4& transform)
{
	auto node = new SceneNode;
	node->MaterialData = material;
	node->Ref.StaticMesh = mesh;
	node->Transform.Local = transform;
	node->Type = NODE_TYPE_STATIC_MESH;

	return node;
}

SceneNode* CreateStaticMeshInstancedNode(StaticMesh* mesh, MaterialData* material, const DirectX::XMFLOAT4X4& transform)
{
	auto node = CreateStaticMeshNode(mesh, material, transform);
	node->Type = NODE_TYPE_STATIC_MESH_INSTANCED;

	return node;
}

SceneNode* CreateLightNode(LightType type, LightData* data)
{
	auto node = new SceneNode;
	node->MaterialData = nullptr;
	node->Ref.LightData = data;
	XMStoreFloat4x4(&node->Transform.Local, XMMatrixIdentity());
	node->Type = NODE_TYPE_LIGHT;

	return node;
}

SceneNode* CreateTerrainPatchNode(TerrainPatch* terrainPatch, const XMFLOAT4X4& transform)
{
	auto node = new SceneNode;
	node->MaterialData = nullptr;
	node->Ref.TerrainPatch = terrainPatch;
	node->Transform.Local = transform;
	node->Type = NODE_TYPE_TERRAIN_PATCH;

	return node;
}
