#include "ShaderHeader.hlsli"

PSOutputForward main(VSOutputStandard input)
{
	PSOutputForward output;
	output.albedo = input.normal.y;
	return output;
}