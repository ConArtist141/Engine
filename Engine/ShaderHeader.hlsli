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

struct VSInputBlit
{
	float2 position : POSITION;
	float2 uv : TEXCOORD;
};

struct VSOutputStandard
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

struct VSOutputBlit
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PSOutputDeferred
{
	float4 albedo : SV_TARGET0;
	float4 normal : SV_TARGET1;
};

struct PSOutputComposite
{
	float4 color : SV_TARGET0;
};