@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;
layout(rgba32f, binding = 1) uniform imageBuffer uPositionQueue;
layout(rgba32f, binding = 2) uniform imageBuffer uThroughputQueue;
layout(rgba32f, binding = 3) uniform imageBuffer uWoQueue;
layout(rg32i, binding = 4) uniform iimageBuffer uUVQueue;
layout(r32i, binding = 5) uniform iimageBuffer uIdQueue;

bool BUG = false;
vec3 BUGVAL;

@include random.glsl
@include math.glsl
@include material.glsl
@include intersection.glsl
@include light.glsl
@include camera.glsl

uniform samplerBuffer uMaterials;
uniform isamplerBuffer uMatTypes;
uniform isamplerBuffer uMatTexIndices;

uniform int uNumTextures;
uniform sampler2DArray uTextures;
uniform samplerBuffer uTexUVScale;

uniform int uSpp;
uniform int uFreeCounter;
uniform int uSampleDim;
uniform sampler2D uNoiseTex;
uniform int uSampleNum;

uniform bool uSampleLight;
uniform bool uRussianRoulette;

uniform int uQueueCapacity;
uniform int uQueueSize;
uniform int uCurDepth;
const int SampleNumPerPass = 9;

@include material_loader.glsl

struct QueueItem
{
	vec3 pos;
	vec3 throughput;
	vec3 wo;
	ivec2 uv;
	int id;
};

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

bool queueEmpty()
{
	return (imageLoad(uIdQueue, 0).x == imageLoad(uIdQueue, 1).x);
}

QueueItem fetchQueue()
{
	QueueItem item;
	int headPtr = imageAtomicAdd(uIdQueue, 0, 1);
	headPtr = ((headPtr % uQueueCapacity) + uQueueCapacity) % uQueueCapacity;

	item.pos = imageLoad(uPositionQueue, headPtr).xyz;
	item.throughput = imageLoad(uThroughputQueue, headPtr).rgb;
	item.wo = imageLoad(uWoQueue, headPtr).xyz;
	item.uv = imageLoad(uUVQueue, headPtr).xy;
	item.id = imageLoad(uIdQueue, headPtr).x;
	return item;
}

void extendPath(QueueItem item, inout Sampler s)
{
	vec3 pos = item.pos;
	vec3 throughput = item.throughput;
	vec3 wo = item.wo;
	ivec2 uv = item.uv;
	int id = item.id;

	SurfaceInfo surf = triangleSurfaceInfo(id, pos);

	int matTexId = texelFetch(uMatTexIndices, id).r;
	int matId = matTexId & 0x0000ffff;
	int texId = matTexId >> 16;

	BSDFType matType = loadMaterialType(matId);
	if (matType != Dielectric && matType != ThinDielectric)
	{
		if (dot(surf.ns, wo) < 0)
			flipNormals(surf);
	}

	BSDFParam matParam = loadMaterial(matType, matId, texId, surf.uv);

	float ud = sample1D(s);
	vec4 us = sample4D(s);
	if (uSampleLight)
	{
		LightLiSample samp = sampleLightAndEnv(pos, ud, us);
		if (samp.pdf > 0.0)
		{
			vec4 bsdfAndPdf = materialBSDFAndPdf(matType, matParam, wo, samp.wi, surf.ns, Radiance);
			float weight = biHeuristic(samp.pdf, bsdfAndPdf.w);
			accumulateImage(uv, bsdfAndPdf.xyz * throughput * satDot(surf.ns, samp.wi) * samp.coef * weight);
		}
	}

	BSDFSample samp = materialSample(matType, matParam, surf.ns, wo, Radiance, sample3D(s));
	vec3 wi = samp.wi;
	float bsdfPdf = samp.pdf;
	vec3 bsdf = samp.bsdf;
	uint flag = samp.flag;

	bool deltaBsdf = (flag == SpecRefl || flag == SpecTrans);

	if (bsdfPdf < 1e-8)
		return;
	throughput *= bsdf / bsdfPdf * (deltaBsdf ? 1.0 : absDot(surf.ns, wi));

	Ray ray = rayOffseted(pos, wi);

	float dist;
	int nextId = bvhHit(ray, dist);
	int lightId = nextId - uObjPrimCount;
	vec3 nextPos = rayPoint(ray, dist);

	if (nextId == -1)
	{
		vec3 radiance = envLe(wi);
		float weight = 1.0;
		if (uSampleLight && !deltaBsdf)
		{
			float envPdf = envPdfLi(wi) * pdfSelectEnv();
			weight = (envPdf <= 0.0) ? 0.0 : biHeuristic(bsdfPdf, envPdf);
		}
		accumulateImage(uv, radiance * throughput * weight);
		return;
	}
	else if (lightId >= 0)
	{
		vec3 radiance = lightLe(lightId, nextPos, -wi);
		float weight = 1.0;
		if (uSampleLight && !deltaBsdf)
		{
			float lightPdf = lightPdfLi(lightId, pos, nextPos) * pdfSelectLight(lightId);
			weight = (lightPdf <= 0.0) ? 0.0 : biHeuristic(bsdfPdf, lightPdf);
		}
		accumulateImage(uv, radiance * throughput * weight);
		return;
	}

	if (uRussianRoulette)
	{
		float continueProb = min(maxComponent(bsdf / bsdfPdf), 0.95f);
		if (sample1D(s) >= continueProb)
			return;
		throughput /= continueProb;
	}

	pushQueue(nextPos, throughput, -wi, uv, nextId);
}

void main()
{
	if (gl_GlobalInvocationID.x >= uQueueSize)
		return;
	if (queueEmpty())
		return;

	QueueItem item = fetchQueue();

	ivec2 coord = item.uv;
	vec2 texSize = vec2(uFilmSize);

	vec2 scrCoord = vec2(coord) / texSize;

	Sampler sampleIdx = 2 + SampleNumPerPass * uCurDepth;

	vec2 noiseCoord = texture(uNoiseTex, scrCoord).xy;
	noiseCoord = texture(uNoiseTex, noiseCoord).xy;
	vec2 texCoord = texSize * noiseCoord;
	randSeed = (uint(texCoord.x) * uFreeCounter) + uint(texCoord.y);
	sampleSeed = uint(texCoord.x) * uint(texCoord.y);

	sampleOffset = uSpp * uSampleDim;
	if (sampleOffset > uSampleNum * uSampleDim)
		sampleOffset -= uSampleNum * uSampleDim;

	extendPath(item, sampleIdx);
}