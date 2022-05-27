@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;

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
uniform int uMaxDepth;

@include material_loader.glsl

struct QueueItem
{
	vec3 pos;
	vec3 throughput;
	vec3 wo;
	ivec2 uv;
	int depth;
	int id;
};

const int QueueCapacity = 32;
shared QueueItem queue[QueueCapacity + 1];
shared int mark[QueueCapacity + 1];
shared int headPtr = 0;
shared int tailPtr = 0;
shared int queueSize = 0;

bool fetchQueue(out QueueItem item)
{
	int curSize = atomicAdd(queueSize, -1);
	if (curSize <= 0)
		return false;
	int offset = atomicAdd(headPtr, 1);
	//atomicAdd(mark[offset], 1);
	offset = ((offset % QueueCapacity) + QueueCapacity) % QueueCapacity;
	item = queue[offset];
	return true;
}

void pushQueue(vec3 pos, vec3 throughput, vec3 wo, ivec2 uv, int depth, int id)
{
	atomicAdd(queueSize, 1);
	int offset = atomicAdd(tailPtr, 1);
	offset = ((offset % QueueCapacity) + QueueCapacity) % QueueCapacity;
	queue[offset].pos = pos;
	queue[offset].throughput = throughput;
	queue[offset].wo = wo;
	queue[offset].uv = uv;
	queue[offset].depth = depth;
	queue[offset].id = id;
}

void accumulateImage(ivec2 iuv, vec3 color)
{
	vec3 last = imageLoad(uFrame, iuv).rgb;
	imageStore(uFrame, iuv, vec4(last + color, 1.0));
}

void processPrimaryRay(Ray ray, ivec2 uv, inout Sampler s)
{
	float primDist;
	int id = bvhHit(ray, primDist);
	vec3 pos = rayPoint(ray, primDist);
	vec3 wo = -ray.dir;

	if (id == -1)
	{
		accumulateImage(uv, envLe(ray.dir));
		return;
	}
	else if (id - uObjPrimCount >= 0)
	{
		accumulateImage(uv, lightLe(id - uObjPrimCount, pos, -ray.dir));
		return;
	}
	pushQueue(pos, vec3(1.0), wo, uv, 1, id);
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
		float continueProb = deltaBsdf ? 0.95 : min(maxComponent(bsdf / bsdfPdf), 0.95);
		if (sample1D(s) >= continueProb)
			return;
		throughput /= continueProb;
	}
	pushQueue(nextPos, throughput, -wi, uv, item.depth + 1, nextId);
}

void main()
{
	int threadId = int(gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x);
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

	if (threadId == 0)
		headPtr = tailPtr = queueSize = 0;

	barrier();

	Ray ray = thinLensCameraSampleRay(scrCoord, sample4D(sampleIdx));
	processPrimaryRay(ray, coord, sampleIdx);
	
	//mark[threadId] = 0;

	/*for (int depth = 1; depth <= uMaxDepth; depth++)
	{
		barrier();
		QueueItem item;
		if (!fetchQueue(item))
			break;
		sampleIdx = 4 + 9 * depth;
		extendPath(item, sampleIdx);
	}*/
	QueueItem item;
	while (fetchQueue(item))
	{
		barrier();
		if (item.depth >= uMaxDepth)
			break;
		sampleIdx = 4 + 9 * item.depth;
		extendPath(item, sampleIdx);
	}
	//barrier();
	//imageStore(uFrame, coord, vec4(colorWheel(float(queueSize) / 512.0), 1.0));
}