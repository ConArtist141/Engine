#include "ShaderHeader.hlsli"

sampler TextureSampler : register(s0);

texture2D Albedo : register(t0);
texture2D Normal : register(t1);

float4 main(VSOutputBlit input) : SV_TARGET
{
	float4 albedo = Albedo.Sample(TextureSampler, input.uv);
	float4 normal = Normal.Sample(TextureSampler, input.uv);
	normal = normal * 2.0f - 1.0f;

	float3 LightDirection = normalize(float3(-1.0f, -1.0f, -1.0f));
	return float4(albedo.rgb * dot(-LightDirection, normal.xyz), 1.0f);
}