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
	float4 normal : SV_TARGET1;
};

struct VSInputBlit
{
	float2 position : POSITION;
	float2 uv : TEXCOORD;
};

struct VSOutputBlit
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};
