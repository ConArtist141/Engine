#ifndef SCENE_GRAPH_H_
#define SCENE_GRAPH_H_

#include <DirectXMath.h>
#include <vector>

#include "StaticMesh.h"
#include "MaterialData.h"
#include "Terrain.h"

class SceneNode;

enum MajorAxis
{
	MAJOR_AXIS_X,
	MAJOR_AXIS_Y,
	MAJOR_AXIS_Z
};

enum NodeType
{
	NODE_TYPE_EMPTY,
	NODE_TYPE_ZONE,
	NODE_TYPE_STATIC_MESH,
	NODE_TYPE_TERRAIN_PATCH,
	NODE_TYPE_LIGHT,
	NODE_TYPE_END_ENUM
};

typedef void (*GetNodeBoundsFunctionPtr)(const SceneNode*, Bounds*);

struct SceneNodeFunctionTableEntry
{
	GetNodeBoundsFunctionPtr GetNodeBounds;
};

extern SceneNodeFunctionTableEntry SceneNodeFunctionTable[];

enum NodeTypeRange
{
	NODE_TYPE_RANGE_MESH_BEGIN = NODE_TYPE_STATIC_MESH,
	NODE_TYPE_RANGE_MESH_END = NODE_TYPE_TERRAIN_PATCH
};

struct NodeTransform
{
	DirectX::XMFLOAT4X4 Local;
	DirectX::XMFLOAT4X4 Global;
};

struct RegionNode
{
	Bounds AABB;
	RegionNode* Node1;
	RegionNode* Node2;
	RegionNode* Node3;
	SceneNode* LeafData;
};

enum LightType
{
	LIGHT_TYPE_DIRECTIONAL,
	LIGHT_TYPE_OMNI
};

struct LightData
{
	LightType Type;
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Direction;
	float Radius;
};

struct ZoneData
{
	std::string Name;
	SceneNode* ZoneDirectionalLight;
};

union NodeRef
{
	StaticMesh* StaticMesh;
	LightData* LightData;
	ZoneData* ZoneData;
	TerrainPatch* TerrainPatch;
};

// A node in a scene graph. Note that the root of a scene must be a zone.
class SceneNode
{
public:
	std::vector<SceneNode*> Children;
	RegionNode Region;
	NodeTransform Transform;
	MaterialData* MaterialData;
	NodeType Type;
	NodeRef Ref;

	inline bool IsZone() const;
	inline bool IsMesh() const;
	inline bool IsStaticMesh() const;
	inline bool IsTerrainPatch() const;
	inline bool IsLight() const;
};

inline bool SceneNode::IsZone() const
{
	return Type == NODE_TYPE_ZONE;
}
inline bool SceneNode::IsMesh() const			
{
	return Type >= NODE_TYPE_RANGE_MESH_BEGIN && Type <= NODE_TYPE_RANGE_MESH_END;
}
inline bool SceneNode::IsStaticMesh() const
{
	return Type == NODE_TYPE_STATIC_MESH;
}
inline bool SceneNode::IsTerrainPatch() const
{
	return Type == NODE_TYPE_TERRAIN_PATCH;
}
inline bool SceneNode::IsLight() const
{
	return Type == NODE_TYPE_LIGHT;
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
SceneNode* CreateStaticMeshNode(StaticMesh* mesh, MaterialData* material, const DirectX::XMFLOAT4X4& transform);
SceneNode* CreateLightNode(LightType type, LightData* data);
SceneNode* CreateTerrainPatchNode(TerrainPatch* terrainPatch);

#endif