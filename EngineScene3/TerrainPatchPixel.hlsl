#include "ShaderHeader.hlsli"

sampler TextureSampler : register(s0);
texture2D Albedo : register(t0);

PSOutputForward main(VSOutputStandard input)
{
	PSOutputForward output;
	output.albedo = Albedo.Sample(TextureSampler, input.uv);
	// output.albedo = float4(input.uv.x, input.uv.y, 0.0f, 1.0f);
	return output;
}