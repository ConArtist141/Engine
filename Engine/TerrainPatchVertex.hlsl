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
	float TextureScale0 = 1.0f / 16.0f;

	VSOutputStandard output;
	float4 worldPosition = mul(World, float4(input.position, 1.0));
	output.position = mul(Projection, mul(View, worldPosition));
	output.normal = mul(World, float4(input.normal, 0.0)).xyz;
	output.uv = worldPosition.xz * TextureScale0;
	return output;
}