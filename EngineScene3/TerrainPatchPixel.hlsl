#include "ShaderHeader.hlsli"

PSOutputForward main(VSOutputStandard input)
{
	PSOutputForward output;
	output.albedo = float4(1.0f, 1.0f, 1.0f, 1.0f);
	return output;
}