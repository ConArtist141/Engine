#include "ShaderHeader.hlsli"

sampler TextureSampler : register(s0);
texture2D Albedo : register(t0);

PSOutputForward main(VSOutputStandard input)
{
	PSOutputForward output;

	output.albedo = Albedo.Sample(TextureSampler, input.uv);

	return output;
}