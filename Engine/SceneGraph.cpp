#include "SceneGraph.h"

#include <stack>
#include <limits>

using namespace DirectX;
using namespace std;

void TransformBounds(const XMMATRIX& matrix, const Bounds& bounds, Bounds* boundsOut)
{
	XMVECTOR vecs[8] =
	{
		XMVectorSet(bounds.Lower.x, bounds.Lower.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Lower.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Upper.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Lower.x, bounds.Upper.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Lower.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Lower.y, bounds.Upper.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Upper.y, bounds.Lower.z, 1.0f),
		XMVectorSet(bounds.Upper.x, bounds.Upper.y, bounds.Upper.z, 1.0f)
	};

	auto infinity = numeric_limits<float>::infinity();
	boundsOut->Lower = { infinity, infinity, infinity };
	boundsOut->Upper = { -infinity, -infinity, -infinity };

	for (size_t i = 0; i < 8; ++i)
	{
		vecs[i] = XMVector4Transform(vecs[i], matrix);

		auto x = XMVectorGetX(vecs[i]);
		auto y = XMVectorGetY(vecs[i]);
		auto z = XMVectorGetZ(vecs[i]);

		if (boundsOut->Lower.x > x)
			boundsOut->Lower.x = x;
		if (boundsOut->Lower.y > y)
			boundsOut->Lower.y = y;
		if (boundsOut->Lower.z > z)
			boundsOut->Lower.z = z;

		if (boundsOut->Upper.x < x)
			boundsOut->Upper.x = x;
		if (boundsOut->Upper.y < y)
			boundsOut->Upper.y = y;
		if (boundsOut->Upper.z < z)
			boundsOut->Upper.z = z;
	}
}

void UpdateTransforms(SceneNode* node, const XMMATRIX& transform)
{
	XMMATRIX matrix = XMLoadFloat4x4(&node->Transform.Local);
	matrix = XMMatrixMultiply(matrix, transform);
	XMStoreFloat4x4(&node->Transform.Global, matrix);

	for (auto child : node->Children)
		UpdateTransforms(child, matrix);
}

void CollectZoneVolumeHierachyLeaves(SceneNode* node, vector<SceneNode*>& leaves)
{
	for (auto child : node->Children)
	{
		// Collect static meshes and zones.
		if (child->IsMesh())
			leaves.push_back(child);
		else if (child->IsZone())
			leaves.push_back(child);
		else
			CollectZoneVolumeHierachyLeaves(child, leaves);
	}
}

void GetVolumeLeafBounds(SceneNode* node, Bounds* boundsOut)
{
	if (node->IsStaticMesh())
	{
		node->Ref.StaticMesh->GetMeshBounds(boundsOut);
		XMMATRIX matrix = XMLoadFloat4x4(&node->Transform.Global);
		TransformBounds(matrix, *boundsOut, boundsOut);
	}
	else if (node->IsZone())
		*boundsOut = node->Region.AABB;
	else
	{
		OutputDebugString("Invalid leaf type!\n");
		assert(1);
	}
}

enum MajorAxis
{
	MAJOR_AXIS_X,
	MAJOR_AXIS_Y,
	MAJOR_AXIS_Z
};

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
		DestroyBoundingVolumeHierarchy(node->LeafData, true);
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

void DestroyBoundingVolumeHierarchy(SceneNode* zone, const bool bDestroyChildrenHierarchies)
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

void BuildBoundingVolumeHierarchy(SceneNode* zone, const bool bRebuildChildrenZones)
{
	if (!zone->IsZone())
	{
		OutputDebugString("Scene node specified is not a zone!\n");
		return;
	}

	DestroyBoundingVolumeHierarchy(zone, bRebuildChildrenZones);

	vector<SceneNode*> leaves;
	CollectZoneVolumeHierachyLeaves(zone, leaves);

	vector<RegionNode*> leafRegions;
	for (auto leaf : leaves)
	{
		Bounds bounds;

		// Rebuild children
		if (bRebuildChildrenZones && leaf->IsZone())
			BuildBoundingVolumeHierarchy(leaf, true);

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
		DestroyBoundingVolumeHierarchy(sceneNode, true);

	for (auto child : sceneNode->Children)
		DestroySceneGraph(child);

	delete sceneNode;
}

SceneNode* CreateStaticMeshNode(StaticMesh* mesh, Material* material, const XMFLOAT4X4& transform)
{
	auto node = new SceneNode;
	node->Material = material;
	node->Ref.StaticMesh = mesh;
	node->Transform.Local = transform;
	node->Type = NODE_TYPE_STATIC_MESH;

	return node;
}

SceneNode* CreateLightNode(LightType type, LightData* data)
{
	auto node = new SceneNode;
	node->Material = nullptr;
	node->Ref.LightData = data;
	XMStoreFloat4x4(&node->Transform.Local, XMMatrixIdentity());
	node->Type = NODE_TYPE_LIGHT;

	return node;
}