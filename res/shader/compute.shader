=type compute
#version 450 core
layout(local_size_x = 48, local_size_y = 32) in;

layout(rgba32f, binding = 0) uniform image2D frame;
//layout(rgba32f, binding = 1) uniform writeonly image2D outImage;

=include rayTest.shader

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	vec2 texSize = vec2(frameSize);

	if (coord.x >= frameSize.x || coord.y >= frameSize.y) return;

	vec2 scrCoord = vec2(coord) / texSize;

	int sampleIdx = 0;
	
	vec2 noiseCoord = texture(noiseTex, scrCoord).xy;
	noiseCoord = texture(noiseTex, noiseCoord).xy;
	vec2 texCoord = texSize * noiseCoord;
	randSeed = (uint(texCoord.x) * freeCounter) + uint(texCoord.y);
	sampleSeed = uint(texCoord.x);

	//sampleOffset = spp + int(texCoord.x) + int(texCoord.y);
	sampleOffset = spp * sampleDim;
	if (sampleOffset > sampleNum) sampleOffset -= sampleNum;

	vec3 result = vec3(0.0);

	Ray ray = sampleLensCamera(scrCoord, sampleIdx);

	result = getResult(ray, sampleIdx);
	vec3 lastRes = imageLoad(frame, coord).rgb;

	if (isnan(result.x) || isnan(result.y) || isnan(result.z)) result = lastRes;
	else result = (lastRes * float(spp) + result) / float(spp + 1);
	if (BUG) result = BUGVAL;

	imageStore(frame, coord, vec4(result, 1.0));
}