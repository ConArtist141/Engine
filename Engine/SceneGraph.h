#ifndef SCENE_GRAPH_H_
#define SCENE_GRAPH_H_

#include <DirectXMath.h>
#include <vector>

#include "StaticMesh.h"
#include "Material.h"

struct NodeTransform
{
	DirectX::XMFLOAT4X4 Local;
	DirectX::XMFLOAT4X4 Global;
};

enum NodeType
{
	NODE_TYPE_EMPTY = 0,
	NODE_TYPE_ZONE = 1,
	NODE_TYPE_STATIC_MESH = 2,
};

enum NodeTypeRange
{
	NODE_TYPE_RANGE_MESH_BEGIN = NODE_TYPE_STATIC_MESH,
	NODE_TYPE_RANGE_MESH_END = NODE_TYPE_STATIC_MESH
};

class SceneNode;

struct RegionNode
{
	Bounds AABB;
	RegionNode* Node1;
	RegionNode* Node2;
	RegionNode* Node3;
	SceneNode* LeafData;
};

union MeshRef
{
	StaticMesh* Static;
};

// A node in a scene graph. Note that the root of a scene must be a zone.
class SceneNode
{
public:
	std::vector<SceneNode*> Children;
	RegionNode Region;
	NodeTransform Transform;
	Material* Material;
	NodeType Type;
	MeshRef Mesh;

	inline bool IsZone() const;
	inline bool IsMesh() const;
	inline bool IsStaticMesh() const;
};

bool SceneNode::IsZone() const
{
	return Type == NODE_TYPE_ZONE;
}
bool SceneNode::IsMesh() const			
{
	return Type >= NODE_TYPE_RANGE_MESH_BEGIN && Type <= NODE_TYPE_RANGE_MESH_END;
}
bool SceneNode::IsStaticMesh() const
{
	return Type == NODE_TYPE_STATIC_MESH;
}

void TransformBounds(const DirectX::XMMATRIX& matrix, const Bounds& bounds, Bounds* boundsOut);
void UpdateTransforms(SceneNode* node, const DirectX::XMMATRIX& transform);
void CollectZoneVolumeHierachyLeaves(SceneNode* node, std::vector<SceneNode*>& leaves);
void GetVolumeLeafBounds(SceneNode* node, Bounds* boundsOut);
void CreateHierarchyFromBlob(std::vector<RegionNode*> regions, RegionNode* baseRegion);
void DestroyHierarchyRegion(RegionNode* node, const bool bDestroyChildrenHierarchies);
void DestroyBoundingVolumeHierarchy(SceneNode* zone, const bool bDestroyChildrenHierarchies);
void BuildBoundingVolumeHierarchy(SceneNode* zone, const bool bRebuildChildrenZones);

SceneNode* CreateSceneGraph();
void DestroySceneGraph(SceneNode* sceneNode);
SceneNode* CreateStaticMeshNode(StaticMesh* mesh, Material* material, const DirectX::XMFLOAT4X4& transform);

#endif