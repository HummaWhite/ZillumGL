@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;

@include rayTest.shader

uniform int uSpp;
uniform int uFreeCounter;
uniform int uSampleDim;
uniform sampler2D uNoiseTex;
uniform int uSampleNum;

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	vec2 texSize = vec2(uFrameSize);

	if (coord.x >= uFrameSize.x || coord.y >= uFrameSize.y) return;

	vec2 scrCoord = vec2(coord) / texSize;

	int sampleIdx = 0;
	
	vec2 noiseCoord = texture(uNoiseTex, scrCoord).xy;
	noiseCoord = texture(uNoiseTex, noiseCoord).xy;
	vec2 texCoord = texSize * noiseCoord;
	randSeed = (uint(texCoord.x) * uFreeCounter) + uint(texCoord.y);
	sampleSeed = uint(texCoord.x) * uint(texCoord.y);

	//sampleOffset = uSpp + int(texCoord.x) + int(texCoord.y);
	sampleOffset = uSpp * uSampleDim;
	if (sampleOffset > uSampleNum * uSampleDim) sampleOffset -= uSampleNum * uSampleDim;

	vec3 result = vec3(0.0);

	Ray ray = sampleLensCamera(scrCoord, sampleIdx);

	result = getResult(ray, sampleIdx);
	vec3 lastRes = imageLoad(uFrame, coord).rgb;

	if (isnan(result.x) || isnan(result.y) || isnan(result.z)) result = lastRes;
	else result = (lastRes * float(uSpp) + result) / float(uSpp + 1);
	if (BUG) result = BUGVAL;

	imageStore(uFrame, coord, vec4(result, 1.0));
}