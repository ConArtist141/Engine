struct VSInputStandard
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
	matrix world : INSTANCE;
};

struct VSOutputStandard
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

struct PSOutputDeferred
{
	float4 albedo : SV_TARGET0;
};