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

vec3 pathIntegTrace(Ray ray, inout Sampler s)
{
	float primDist;
	int id = bvhHit(ray, primDist);
	vec3 pos = rayPoint(ray, primDist);

	if (id == -1)
		return envLe(ray.dir);
	else if (id - uObjPrimCount >= 0)
		return lightLe(id - uObjPrimCount, pos, -ray.dir);

	vec3 wo = -ray.dir;

	vec3 result = vec3(0.0);
	vec3 throughput = vec3(1.0);
	float etaScale = 1.0;

	for (int bounce = 1; bounce <= uMaxDepth; bounce++)
	{
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

		if (uSampleLight)
		{
			float ud = sample1D(s);
			vec4 us = sample4D(s);

			LightLiSample samp = sampleLightAndEnv(pos, ud, us);
			if (samp.pdf > 0.0)
			{
				vec4 bsdfAndPdf = materialBSDFAndPdf(matType, matParam, wo, samp.wi, surf.ns, Radiance);
				float weight = biHeuristic(samp.pdf, bsdfAndPdf.w);
				result += bsdfAndPdf.xyz * throughput * satDot(surf.ns, samp.wi) * samp.coef * weight;
			}
		}

		BSDFSample samp = materialSample(matType, matParam, surf.ns, wo, Radiance, sample3D(s));
		vec3 wi = samp.wi;
		float bsdfPdf = samp.pdf;
		vec3 bsdf = samp.bsdf;
		uint flag = samp.flag;

		bool deltaBsdf = (flag == SpecRefl || flag == SpecTrans);

		if (bsdfPdf < 1e-8)
			break;
		throughput *= bsdf / bsdfPdf * (deltaBsdf ? 1.0 : absDot(surf.ns, wi));

		ray = rayOffseted(pos, wi);

		float dist;
		int primId = bvhHit(ray, dist);
		int lightId = primId - uObjPrimCount;
		vec3 nextPos = rayPoint(ray, dist);

		if (primId == -1)
		{
			vec3 radiance = envLe(wi);
			float weight = 1.0;
			if (uSampleLight && !deltaBsdf)
			{
				float envPdf = envPdfLi(wi) * pdfSelectEnv();
				weight = (envPdf <= 0.0) ? 0.0 : biHeuristic(bsdfPdf, envPdf);
			}
			result += radiance * throughput * weight;
			break;
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
			result += radiance * throughput * weight;
			break;
		}

		if (uRussianRoulette)
		{
			float continueProb = min(maxComponent(bsdf / bsdfPdf), 0.95f);
			if (sample1D(s) >= continueProb)
				break;
			throughput /= continueProb;
		}

		if (flag == SpecTrans || flag == GlosTrans)
			etaScale *= square(samp.eta);

		id = primId;
		pos = nextPos;
		wo = -wi;
	}
	return result;
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

	//sampleOffset = uSpp + int(texCoord.x) + int(texCoord.y);
	sampleOffset = uSpp * uSampleDim;
	if (sampleOffset > uSampleNum * uSampleDim) sampleOffset -= uSampleNum * uSampleDim;

	Ray ray = thinLensCameraSampleRay(scrCoord, sample4D(sampleIdx));
	vec3 result = pathIntegTrace(ray, sampleIdx);
	if (BUG) result = BUGVAL;

	vec3 lastRes = (uSpp == 0) ? vec3(0.0) : imageLoad(uFrame, coord).rgb;

	if (!hasNan(result))
		imageStore(uFrame, coord, vec4(lastRes + result, 1.0));
}