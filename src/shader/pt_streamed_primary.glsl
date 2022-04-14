@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;
layout(rgba32f, binding = 1) uniform imageBuffer uPositionQueue;
layout(rgba32f, binding = 2) uniform imageBuffer uThroughputQueue;
layout(rgba32f, binding = 3) uniform imageBuffer uWoQueue;
layout(rg32i, binding = 4) uniform iimageBuffer uUVQueue;
layout(r32i, binding = 5) uniform iimageBuffer uIdQueue;

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
uniform int uQueueCapacity;

void accumulateImage(ivec2 iuv, vec3 color)
{
	vec3 last = imageLoad(uFrame, iuv).rgb;
	imageStore(uFrame, iuv, vec4(last + color, 1.0));
}

void pushQueue(vec3 pos, vec3 throughput, vec3 wo, ivec2 uv, int id)
{
	int tailPtr = imageAtomicAdd(uIdQueue, 1, 1);
	tailPtr = ((tailPtr % uQueueCapacity) + uQueueCapacity) % uQueueCapacity;
	imageStore(uPositionQueue, tailPtr, vec4(pos, 0.0));
	imageStore(uThroughputQueue, tailPtr, vec4(throughput, 0.0));
	imageStore(uWoQueue, tailPtr, vec4(wo, 1.0));
	imageStore(uUVQueue, tailPtr, ivec4(uv, 0, 0));
	imageStore(uIdQueue, tailPtr + 2, ivec4(id, 0, 0, 0));
}

bool processPrimaryRay(Ray ray, ivec2 uv, out vec3 result)
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
	pushQueue(pos, vec3(1.0), wo, uv, id);
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
	bool surfaceHit = processPrimaryRay(ray, coord, result);

	if (!surfaceHit)
		accumulateImage(coord, result);
}