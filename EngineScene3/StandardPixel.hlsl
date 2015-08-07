#include "ShaderHeader.hlsli"

sampler TextureSampler : register(s0);
texture2D Albedo : register(t0);

PSOutputDeferred main(VSOutputStandard input)
{
	PSOutputDeferred output;

	output.albedo = Albedo.Sample(TextureSampler, input.uv);
	output.normal = float4(input.normal * 0.5f + 0.5f, 1.0f);

	return output;
}