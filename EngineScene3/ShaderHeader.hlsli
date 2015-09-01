struct VSInputStaticMeshInstanced
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
	matrix world : INSTANCE;
};

struct VSInputStaticMesh
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

struct VSInputTerrainPatch
{
	float3 position : POSITION;
	float3 normal : NORMAL;
};

struct VSOutputStandard
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

struct PSOutputForward
{
	float4 albedo : SV_TARGET0;
};