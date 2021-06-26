=type vertex
#version 450 core
layout(location = 0) in vec2 pos;

out vec2 scrCoord;

void main()
{
	vec2 ndc = pos * 2.0 - 1.0;
	gl_Position = vec4(ndc, 0.0, 1.0);
	scrCoord = pos;
}

=type fragment
#version 450 core
in vec2 scrCoord;
out vec4 FragColor;

uniform sampler2D lastFrame;

=include rayTest.shader

void main()
{
	int sampleIdx = 0;

	vec2 texSize = textureSize(lastFrame, 0).xy;
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
	vec3 lastRes = texture(lastFrame, scrCoord).rgb;

	if (isnan(result.x) || isnan(result.y) || isnan(result.z)) result = lastRes;
	else result = (lastRes * float(spp) + result) / float(spp + 1);
	if (BUG) result = BUGVAL;

	FragColor = vec4(result, 1.0);
}