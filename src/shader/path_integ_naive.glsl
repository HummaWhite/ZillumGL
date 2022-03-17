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
uniform int uMaxBounce;

uint getMaterialType(int matId)
{
	return texelFetch(uMatTypes, matId * 4 + 3).w;
}

MetalWorkflowBRDF loadMetalWorkflow(int matId, int texId, vec2 uv)
{
	MetalWorkflowBRDF ret;
	if (texId == -1)
		ret.baseColor = texelFetch(uMaterials, matId * 4 + 0).xyz;
	else
	{
		vec2 uvScale = texelFetch(uTexUVScale, texId).xy;
		ret.baseColor = texture2DArray(uTextures, vec3(fract(uv) * uvScale, texId)).rgb;
	}
	vec2 metRough = texelFetch(uMaterials, matId * 4 + 1).xy;
	ret.metallic = mix(0.0134, 1.0, metRough.x);
	ret.roughness = metRough.y;
	return ret;
}

PrincipledBRDF loadPrincipledBRDF(int matId, int texId, vec2 uv)
{
	PrincipledBRDF ret;
	vec4 baseColSS = texelFetch(uMaterials, matId * 4 + 0);
	vec4 metRouSpecTint = texelFetch(uMaterials, matId * 4 + 1);
	vec4 sheenTintCoatGloss = texelFetch(uMaterials, matId * 4 + 2);

	ret.baseColor = (texId == -1) ? baseColSS.rgb :
		texture2DArray(uTextures, vec3(fract(uv) * texelFetch(uTexUVScale, texId).xy, texId)).rgb;
	ret.subsurface = baseColSS.w;
	ret.metallic = metRouSpecTint.x;
	ret.roughness = mix(0.0134, 1.0, metRouSpecTint.y);
	ret.specular = metRouSpecTint.z;
	ret.specularTint = metRouSpecTint.w;
	ret.sheen = sheenTintCoatGloss.x;
	ret.sheenTint = sheenTintCoatGloss.y;
	ret.clearcoat = sheenTintCoatGloss.z;
	ret.clearcoatGloss = sheenTintCoatGloss.w;
	return ret;
}

vec3 pathIntegTrace(Ray ray, int id, SurfaceInfo surf, inout Sampler s)
{
	vec3 result = vec3(0.0);
	vec3 beta = vec3(1.0);
	float etaScale = 1.0;

	for (int bounce = 0; bounce <= uMaxBounce; bounce++)
	{
		vec3 pos = ray.ori;
		vec3 wo = -ray.dir;
		vec3 norm = surf.norm;

		//if (dot(norm, wo) < 0) norm = -norm;
		int matTexId = texelFetch(uMatTexIndices, id).r;
		int matId = matTexId & 0x0000ffff;
		int texId = matTexId >> 16;

		PrincipledBRDF matParam = loadPrincipledBRDF(matId, texId, surf.uv);

		/*MetalWorkflowBRDF matParam = loadMetalWorkflow(matId, texId, surf.uv);
		vec3 baseColor = matParam.baseColor;
		float metallic = matParam.metallic;
		float roughness = matParam.roughness;*/

		if (uSampleLight)
		{
			float ud = sample1D(s);
			vec4 us = sample4D(s);

			LightLiSample samp = sampleLightAndEnv(pos, ud, us);
			if (samp.pdf > 0.0)
			{
				float bsdfPdf = principledPdf(wo, samp.wi, norm, matParam);
				//float bsdfPdf = metalWorkflowPdf(wo, samp.wi, norm, metallic, roughness * roughness);
				float weight = biHeuristic(samp.pdf, bsdfPdf);
				result +=
					principledBsdf(wo, samp.wi, norm, matParam)
					//metalWorkflowBsdf(wo, samp.wi, norm, baseColor, metallic, roughness)
					* beta * satDot(norm, samp.wi) * samp.coef * weight;
			}
		}

		float uf = sample1D(s);
		vec2 u = sample2D(s);
		BsdfSample samp;
		samp = principledSample(norm, wo, matParam, uf, u);
		//samp = metalWorkflowSample(norm, wo, baseColor, metallic, roughness, uf, u);

		vec3 wi = samp.wi;
		float bsdfPdf = samp.pdf;
		vec3 bsdf = samp.bsdf;
		uint flag = samp.flag;

		bool deltaBsdf = (flag == SpecRefl || flag == SpecTrans);

		if (bsdfPdf < 1e-8) break;
		vec3 lastBeta = beta;
		beta *= bsdf / bsdfPdf * (deltaBsdf ? 1.0 : absDot(norm, wi));

		Ray newRay = rayOffseted(pos, wi);

		float dist;
		int primId = bvhHit(newRay, dist);
		int lightId = primId - uObjPrimCount;
		vec3 hitPoint = rayPoint(newRay, dist);

		if (primId == -1)
		{
			vec3 radiance = envLe(wi);
			float weight = 1.0;
			if (uSampleLight && !deltaBsdf)
			{
				float envPdf = envPdfLi(wi) * pdfSelectEnv();
				weight = (envPdf <= 0.0) ? 0.0 : biHeuristic(bsdfPdf, envPdf);
			}
			result += radiance * beta * weight;
			break;
		}
		else if (lightId >= 0)
		{
			vec3 radiance = lightLe(lightId, hitPoint, -wi);
			float weight = 1.0;
			if (uSampleLight && !deltaBsdf)
			{
				float lightPdf = lightPdfLi(lightId, pos, hitPoint) * pdfSelectLight(lightId);
				weight = (lightPdf <= 0.0) ? 0.0 : biHeuristic(bsdfPdf, lightPdf);
			}
			result += radiance * beta * weight;
			break;
		}

		if (uRussianRoulette)
		{
			float continueProb = min(maxComponent(bsdf / bsdfPdf), 0.95f);
			if (sample1D(s) >= continueProb)
				break;
			beta /= continueProb;
		}

		if (flag == SpecTrans || flag == GlosTrans)
			etaScale *= square(samp.eta);

		surf = triangleSurfaceInfo(primId, hitPoint);
		id = primId;
		newRay.ori = hitPoint;
		ray = newRay;
	}
	return result;
}

vec3 getResult(Ray ray, inout Sampler s)
{
	float dist;
	int primId = bvhHit(ray, dist);
	int lightId = primId - uObjPrimCount;
	vec3 hitPoint = rayPoint(ray, dist);

	if (primId == -1)
		return envLe(ray.dir);
	else if (lightId >= 0)
		return lightLe(primId - uObjPrimCount, hitPoint, -ray.dir);
	else
	{
		ray.ori = hitPoint;
		SurfaceInfo surf = triangleSurfaceInfo(primId, ray.ori);
		return pathIntegTrace(ray, primId, surf, s);
	}
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

	Ray ray = thinLensCameraSampleRay(scrCoord, sampleIdx);
	vec3 result = getResult(ray, sampleIdx);
	if (BUG) result = BUGVAL;

	vec3 lastRes = (uSpp == 0) ? vec3(0.0) : imageLoad(uFrame, coord).rgb;

	if (!hasNan(result))
		imageStore(uFrame, coord, vec4(lastRes + result, 1.0));
}