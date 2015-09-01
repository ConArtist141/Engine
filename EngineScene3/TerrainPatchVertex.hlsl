#include "ShaderHeader.hlsli"

cbuffer Constants : register(b0)
{
	matrix World;
	matrix View;
	matrix Projection;
};

VSOutputStandard main(VSInputTerrainPatch input)
{
	VSOutputStandard output;
	output.position = mul(Projection, mul(View, mul(World, float4(input.position, 1.0))));
	output.normal = mul(World, float4(input.normal, 0.0)).xyz;
	output.uv = float2(0.0f, 0.0f);
	return output;
}