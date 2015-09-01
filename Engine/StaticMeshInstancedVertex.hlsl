#include "ShaderHeader.hlsli"

cbuffer Constants : register(b0)
{
	matrix View;
	matrix Projection;
};

VSOutputStandard main(VSInputStaticMeshInstanced input)
{
	VSOutputStandard output;
	output.position = mul(Projection, mul(View, mul(input.world, float4(input.position, 1.0))));
	output.normal = mul(input.world, float4(input.normal, 0.0)).xyz;
	output.uv = input.uv;
	return output;
}