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

VSOutputStandard main(VSInputStaticMesh input)
{
	VSOutputStandard output;
	float4 worldPosition = mul(World, float4(input.position, 1.0));
	output.position = mul(Projection, mul(View, worldPosition));
	output.normal = mul(World, float4(input.normal, 0.0)).xyz;
	output.uv = input.uv;
	return output;
}