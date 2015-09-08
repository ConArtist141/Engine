#include "ShaderHeader.hlsli"

sampler TextureSampler : register(s0);

texture2D Albedo : register(t0);
texture2D Normal : register(t1);
texture2D Depth : register(t2);

PSOutputComposite main(VSOutputBlit input)
{
	float3 lightDirection = normalize(float3(-1.0f, -1.0f, -1.0f));

	PSOutputComposite output;

	float alpha = 1.0f / (0.9975f - 1.0f);
	float beta = -alpha;
	float fog = saturate(alpha * Depth.Sample(TextureSampler, input.uv) + beta);

	float3 normal = Normal.Sample(TextureSampler, input.uv).xyz * 2.0f - 1.0f;
	float light = saturate(-dot(normal, lightDirection));
	light = max(light, 0.25f);

	output.color = Albedo.Sample(TextureSampler, input.uv) * fog * light;

	return output;
}