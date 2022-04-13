@type compute

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

@include material_loader.glsl

void addFilm(ivec2 iuv, vec3 val)
{
	float r = imageLoad(uFrame, ivec2(iuv.x * 3 + 0, iuv.y)).r;
	float g = imageLoad(uFrame, ivec2(iuv.x * 3 + 1, iuv.y)).r;
	float b = imageLoad(uFrame, ivec2(iuv.x * 3 + 2, iuv.y)).r;

	imageStore(uFrame, ivec2(iuv.x * 3 + 0, iuv.y), vec4(r + val.r, 0.0, 0.0, 1.0));
	imageStore(uFrame, ivec2(iuv.x * 3 + 1, iuv.y), vec4(g + val.g, 0.0, 0.0, 1.0));
	imageStore(uFrame, ivec2(iuv.x * 3 + 2, iuv.y), vec4(b + val.b, 0.0, 0.0, 1.0));
}

float remap(float p)
{
	return p < 1e-8 ? 1.0 : p * p;
}

float weightS0(float s1s0, float t1s0)
{
	return 1.0 / (1.0 + s1s0 + t1s0);
}

float weightS1(float s1s0, float t1s1)
{
	return s1s0 / (1.0 + s1s0 + s1s0 * t1s1);
}

vec3 traceCameraPath(Ray ray, inout Sampler s)
{
	float primDist;
	int id = bvhHit(ray, primDist);
	vec3 pos = rayPoint(ray, primDist);

	if (id == -1)
		return envLe(ray.dir);
	else if (id - uObjPrimCount >= 0)
		return lightLe(id - uObjPrimCount, pos, -ray.dir);

	SurfaceInfo surf = triangleSurfaceInfo(id, pos);
	vec3 wo = -ray.dir;
	vec3 prevPos = ray.ori;
	vec3 prevNorm = uCamF;

	vec3 result = vec3(0.0);
	vec3 throughput = vec3(1.0);
	float etaScale = 1.0;

	CameraPdf camPdf = thinLensCameraPdfIe(ray);
	float primaryPdf = remap(camPdf.pdfPos) / remap(camPdf.pdfDir * absDot(surf.ns, ray.dir) / square(primDist));
	float t1s0 = primaryPdf;
	float t1s1 = primaryPdf;

	for (int bounce = 1; bounce <= uMaxDepth; bounce++)
	{
		if (bounce > 1)
			surf = triangleSurfaceInfo(id, pos);

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
			int light = lightSampleOne(sample2D(s));
			int triId = light + uObjPrimCount;
			float pdfSource = lightPdfSampleOne(light);

			vec3 pLit = triangleSampleUniform(triId, sample2D(s));
			vec3 wi = normalize(pLit - pos);
			vec3 Le = lightLe(light, pLit, -wi);
			if (!isBlack(Le) && visible(pos, pLit))
			{
				vec3 nLit = triangleSurfaceInfo(triId, pLit).ng;
				float pA = pdfSource / triangleArea(triId);
				float dist2 = distSquare(pos, pLit);
				float pS = pA * dist2 / absDot(nLit, wi);

				vec4 bsdfAndPdf = materialBSDFAndPdf(matType, matParam, wo, wi, surf.ns, Radiance);
				float pdfRev = materialPdf(matType, matParam, wi, wo, surf.ns, Importance);

				float pdfPLit = remap(pA);
				float coefToSurf = remap(0.5f * PiInv * absDot(surf.ns, wi));
				float coefToLight = remap(bsdfAndPdf.w * satDot(nLit, -wi));
				float coefToPrev = (bounce == 1) ? 1.0 : remap(pdfRev * absDot(prevNorm, wo));
				float coefDist = remap(dist2);
				float weight = weightS1(pdfPLit * coefDist / coefToLight, t1s1 * coefToSurf * coefToPrev / coefDist);

				result += Le * bsdfAndPdf.xyz * throughput * absDot(surf.ns, wi) / pS * weight;
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

		Ray nextRay = rayOffseted(pos, wi);
		float dist;
		int nextId = bvhHit(nextRay, dist);
		int lightId = nextId - uObjPrimCount;
		vec3 nextPos = rayPoint(nextRay, dist);

		float pdfDirToNext = materialPdf(matType, matParam, wo, wi, surf.ns, Radiance);
		float pdfDirToPrev = materialPdf(matType, matParam, wi, wo, surf.ns, Importance);

		if (nextId == -1)
		{
			break;
		}
		else if (lightId >= 0)
		{
			vec3 nLit = triangleSurfaceInfo(nextId, nextPos).ng;
			LightPdf pdfLit = lightPdfLe(lightId, makeRay(nextPos, -wi));
			float pdfPLit = remap(pdfLit.pdfPos * lightPdfSampleOne(lightId));
			float coefToLight = remap(pdfDirToNext * satDot(nLit, -wi));
			float coefToSurf = remap(pdfLit.pdfDir * absDot(surf.ns, wi));
			float coefToPrev = (bounce == 1) ? 1.0 : remap(pdfDirToPrev * absDot(prevNorm, wo));
			float coefDist = remap(dist * dist);
			float weight = isnan(t1s0) ? 0 : weightS0(pdfPLit * coefDist / coefToLight,
				t1s0 * coefToSurf * pdfPLit * coefToPrev / coefToLight);
			
			result += lightLe(lightId, nextPos, -wi) * throughput * weight;
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

		float coef = ((bounce == 1) ? 1.0 : remap(pdfDirToPrev * absDot(prevNorm, wo))) /
			remap(pdfDirToNext * absDot(triangleNormalShad(nextId, nextPos), wi));
		t1s0 *= coef;
		t1s1 *= coef;

		prevPos = pos;
		prevNorm = surf.ns;
		pos = nextPos;
		wo = -wi;
		id = nextId;
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
	vec3 result = traceCameraPath(ray, sampleIdx);
	if (BUG) result = BUGVAL;

	if (!hasNan(result))
		addFilm(coord, result);
}