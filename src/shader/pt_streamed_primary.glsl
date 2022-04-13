@type compute

layout(r32f, binding = 0) uniform image2D uFrame;

layout(rgba32f, binding = 2) uniform imageBuffer uPositionQueue;
layout(rgba32f, binding = 3) uniform imageBuffer uThroughputQueue;
layout(rgba32f, binding = 4) uniform imageBuffer uWoQueue;
layout(r32i, binding = 5) uniform iimageBuffer uObjIdQueue;

@include random.glsl
@include math.glsl
@include intersection.glsl
@include light.glsl
@include camera.glsl

uniform int uSpp;
uniform int uFreeCounter;
uniform int uSampleDim;
uniform sampler2D uNoiseTex;
uniform int uSampleNum;

void accumulateImage(ivec2 iuv, vec3 color)
{
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 0, iuv.y), color.r);
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 1, iuv.y), color.g);
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 2, iuv.y), color.b);
}

void pushQueue(vec3 pos, vec3 throughput, vec3 wo, int id)
{
	int tailPtr = imageAtomicAdd(uObjIdQueue, 1, 1);
	imageStore(uPositionQueue, tailPtr, vec4(pos, 0.0));
	imageStore(uThroughputQueue, tailPtr, vec4(throughput, 0.0));
	imageStore(uWoQueue, tailPtr, vec4(wo, 1.0));
	imageStore(uObjIdQueue, tailPtr + 2, ivec4(id, 0.0, 0.0, 0.0));
}

bool processPrimaryRay(Ray ray, out vec3 result)
{
	float primDist;
	int id = bvhHit(ray, primDist);
	vec3 pos = rayPoint(ray, primDist);
	vec3 wo = -ray.dir;

	if (id == -1)
	{
		result = envLe(ray.dir);
		return false;
	}
	else if (id - uObjPrimCount >= 0)
	{
		result = lightLe(id - uObjPrimCount, pos, -ray.dir);
		return false;
	}
	pushQueue(pos, vec3(1.0), wo, id);
	return true;
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	vec2 texSize = vec2(uFilmSize);

	if (coord.x >= uFilmSize.x || coord.y >= uFilmSize.y)
		return;
	vec2 scrCoord = vec2(coord) / texSize;

	Sampler sampleIdx = 0;

	vec2 noiseCoord = texture(uNoiseTex, scrCoord).xy;
	noiseCoord = texture(uNoiseTex, noiseCoord).xy;
	vec2 texCoord = texSize * noiseCoord;
	randSeed = (uint(texCoord.x) * uFreeCounter) + uint(texCoord.y);
	sampleSeed = uint(texCoord.x) * uint(texCoord.y);

	sampleOffset = uSpp * uSampleDim;
	if (sampleOffset > uSampleNum * uSampleDim) sampleOffset -= uSampleNum * uSampleDim;

	Ray ray = thinLensCameraSampleRay(scrCoord, sample4D(sampleIdx));

	vec3 result;
	bool surfaceHit = processPrimaryRay(ray, result);

	if (!surfaceHit)
		accumulateImage(coord, result);
}