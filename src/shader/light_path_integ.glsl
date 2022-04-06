@type compute

//layout(rgba32f, binding = 0) uniform image2D uFrame;
layout(r32f, binding = 0) uniform image2D uFrame;
bool BUG = false;
vec3 BUGVAL = vec3(0.0);

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

uniform bool uRussianRoulette;
uniform int uMaxDepth;
uniform int uBlocksOnePass;

@include material_loader.glsl

void accumulateFilm(vec2 uv, vec3 res)
{
	if (!inFilmBound(uv))
		return;
	ivec2 iuv = ivec2(uv * vec2(uFilmSize));

	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 0, iuv.y), res.r);
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 1, iuv.y), res.g);
	imageAtomicAdd(uFrame, ivec2(iuv.x * 3 + 2, iuv.y), res.b);
}

void lightIntegTrace(inout Sampler s)
{
	int light = lightSampleOne(sample2D(s));
	float pdfSource = lightPdfSampleOne(light);

	Ray ray;
	vec3 wo;
	vec3 throughput;

	{
		int triId = light + uObjPrimCount;
		vec3 pLit = triangleSampleUniform(triId, sample2D(s));

		CameraIiSample ciSamp = thinLensCameraSampleIi(pLit, sample2D(s));

		if (ciSamp.pdf > 0)
		{
			vec3 pCam = pLit + ciSamp.wi * ciSamp.dist;
			float pdfPos = 1.0 / triangleArea(triId);
			if (visible(pLit, pCam))
			{
				vec3 Le = lightLe(light, pLit, ciSamp.wi);
				vec3 contrib = Le * ciSamp.Ii / (ciSamp.pdf * pdfPos * pdfSource);
				if (!isBlack(contrib))
					accumulateFilm(ciSamp.uv, contrib);
			}
		}

		LightLeSample leSamp = lightSampleOneLe(light, sample4D(s));
		vec3 nl = triangleSurfaceInfo(triId, leSamp.ray.ori).ng;

		wo = -leSamp.ray.dir;
		ray = rayOffseted(leSamp.ray);
		throughput = leSamp.Le * absDot(nl, -wo) / (pdfSource * leSamp.pdfPos * leSamp.pdfDir);
	}

	for (int bounce = 0; bounce <= uMaxDepth; bounce++)
	{
		float dist;
		int id = bvhHit(ray, dist);
		if (id == -1)
			break;
		if (id - uObjPrimCount >= 0)
			break;

		vec3 pos = rayPoint(ray, dist);
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

		{
			CameraIiSample ciSamp = thinLensCameraSampleIi(pos, sample2D(s));
			if (ciSamp.pdf > 0)
			{
				vec3 pCam = pos + ciSamp.wi * ciSamp.dist;
				if (visible(pos, pCam))
				{
					vec3 bsdf = materialBSDF(matType, matParam, wo, ciSamp.wi, surf.ns, Importance);
					float cosWi = satDot(surf.ng, ciSamp.wi) * abs(dot(surf.ns, wo) / dot(surf.ng, wo));
					vec3 res = ciSamp.Ii * bsdf * throughput * cosWi / ciSamp.pdf;
					if (BUG)
						res = BUGVAL;
					if (!hasNan(res) && !isnan(ciSamp.pdf) && ciSamp.pdf > 1e-8 && !isBlack(res))
						accumulateFilm(ciSamp.uv, res);
				}
			}
		}

		BSDFSample samp = materialSample(matType, matParam, surf.ns, wo, Importance, sample3D(s));
		vec3 wi = samp.wi;
		float bsdfPdf = samp.pdf;
		vec3 bsdf = samp.bsdf;
		uint flag = samp.flag;
		bool deltaBsdf = (flag == SpecRefl || flag == SpecTrans);

		if (bsdfPdf < 1e-8 || isnan(bsdfPdf))
			break;
		if (uRussianRoulette)
		{
			float continueProb = min(maxComponent(bsdf / bsdfPdf), 1.0);
			if (sample1D(s) >= continueProb)
				break;
			throughput /= continueProb;
		}

		float cosWi = deltaBsdf ? 1.0 : abs(dot(surf.ng, wi) * dot(surf.ns, wo) / dot(surf.ng, wo));
		throughput *= bsdf * cosWi / bsdfPdf;
		ray = rayOffseted(pos, wi);
		wo = -wi;
	}
}

void main()
{
	uint id = gl_GlobalInvocationID.x;
	Sampler sampleIdx = 0;

	setRngSeed(uSpp * (1536 * uBlocksOnePass) + id + uFreeCounter);

	sampleOffset = uSpp * uSampleDim;
	if (sampleOffset > uSampleNum * uSampleDim) sampleOffset -= uSampleNum * uSampleDim;

	lightIntegTrace(sampleIdx);
}