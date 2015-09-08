#include "ShaderHeader.hlsli"

VSOutputBlit main(VSInputBlit input)
{
	VSOutputBlit output;

	output.position = float4(input.position, 0.0f, 1.0f);
	output.uv = input.uv;

	return output;
}