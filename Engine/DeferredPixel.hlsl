#include "ShaderHeader.hlsli"

sampler TextureSampler : register(s0);

texture2D Albedo : register(t0);
texture2D Normal : register(t1);
texture2D Depth : register(t2);
texture2D Light : register(t3);

PSOutputComposite main(VSOutputBlit input)
{
	float3 lightDirection = normalize(float3(-1.0f, -1.0f, -1.0f));

	PSOutputComposite output;

	float kappa = 0.999f;
	float alpha = 1.0f / (kappa - 1.0f);
	float beta = -alpha;
	float fog = saturate(alpha * Depth.Sample(TextureSampler, input.uv).r + beta);

	float3 light = Light.Sample(TextureSampler, input.uv).rgb;

	output.color = float4((Albedo.Sample(TextureSampler, input.uv).rgb * light) * fog, 1.0f);

	return output;
}