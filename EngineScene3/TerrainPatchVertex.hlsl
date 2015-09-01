#include "ShaderHeader.hlsli"

cbuffer CameraConstants : register(b0)
{
	matrix View;
	matrix Projection;
};

cbuffer InstanceConstants : register(b1)
{
	matrix World;
}

VSOutputStandard main(VSInputTerrainPatch input)
{
	VSOutputStandard output;
	output.position = mul(Projection, mul(View, mul(World, float4(input.position, 1.0))));
	output.normal = mul(World, float4(input.normal, 0.0)).xyz;
	output.uv = float2(0.0f, 0.0f);
	return output;
}