@type lib

bool BUG = false;
vec3 BUGVAL;

@include random.shader
@include math.shader
@include material.shader
@include intersection.shader
@include envMap.shader
@include light.shader

uniform vec3 uCamF;
uniform vec3 uCamR;
uniform vec3 uCamU;
uniform vec3 uCamPos;
uniform float uTanFOV;
uniform float uLensRadius;
uniform float uFocalDist;
uniform float uCamAsp;

uniform bool uShowBVH;
uniform int uMatIndex;
uniform float uBvhDepth;
uniform int uMaxBounce;

uniform bool uAoMode;
uniform vec3 uAoCoef;
uniform ivec2 uFrameSize;

uniform samplerBuffer uMaterials;
uniform isamplerBuffer uMatTypes;
uniform isamplerBuffer uMatTexIndices;

uniform int uNumTextures;
uniform sampler2DArray uTextures;
uniform samplerBuffer uTexUVScale;

uniform bool uSampleLight;

vec3 trace(Ray ray, int id, SurfaceInfo surfaceInfo, inout Sampler s)
{
	vec3 result = vec3(0.0);
	vec3 beta = vec3(1.0);
	float etaScale = 1.0;

	for (int bounce = 0; bounce < uMaxBounce; bounce++)
	{
		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;

		//if (dot(N, Wo) < 0) N = -N;
		// [
		int matTexId = texelFetch(uMatTexIndices, id).r;
		int matId = matTexId & 0x0000ffff;
		int texId = matTexId >> 16;
		//return (N + 1.0) * 0.5;
		// [albedo     metallic/IOR     roughness     type]
		//   vec3        float           float        uint
		vec3 albedo;
		if (texId == -1)
			albedo = texelFetch(uMaterials, matId * 2).rgb;
		else
		{
			vec2 uvScale = texelFetch(uTexUVScale, texId).xy;
			albedo = texture2DArray(uTextures, vec3(fract(surfaceInfo.texCoord) * uvScale, texId)).rgb;
		}
		vec2 tmp = texelFetch(uMaterials, matId * 2 + 1).rg;
		float metIor = tmp.r;
		float roughness = mix(0.0132, 1.0, tmp.g);
		uint type = texelFetch(uMatTypes, matId * 2 + 1).b;

		if (uSampleLight)
		{
			float ud = sample1D(s);
			vec4 us = sample4D(s);

			if (type == Dielectric)
			{
				if (!approximateDelta(roughness))
				{
					LightSample samp = sampleLightAndEnv(P, ud, us);
					if (samp.pdf > 0.0)
					{
						float bsdfPdf = dielectricPdf(Wo, samp.Wi, N, metIor, roughness);
						float weight = biHeuristic(samp.pdf, bsdfPdf);
						result += dielectricBsdf(Wo, samp.Wi, N, albedo, metIor, roughness) * beta * satDot(N, samp.Wi) * samp.coef * weight;
					}
				}
			}
			else if (type == MetalWorkflow)
			{
				LightSample samp = sampleLightAndEnv(P, ud, us);
				if (samp.pdf > 0.0)
				{
					float bsdfPdf = metalWorkflowPdf(Wo, samp.Wi, N, metIor, roughness * roughness);
					float weight = biHeuristic(samp.pdf, bsdfPdf);
					result += metalWorkflowBsdf(Wo, samp.Wi, N, albedo, metIor, roughness) * beta * satDot(N, samp.Wi) * samp.coef * weight;
				}
			}
		}

		float uf = sample1D(s);
		vec2 u = sample2D(s);
		BsdfSample samp;
		switch (type)
		{
		case MetalWorkflow:
			samp = metalWorkflowGetSample(N, Wo, albedo, metIor, roughness, uf, u);
			break;
		case Dielectric:
			samp = dielectricGetSample(N, Wo, albedo, metIor, roughness, uf, u);
			break;
		case ThinDielectric:
			samp = thinDielectricGetSample(N, Wo, albedo, metIor, uf, u);
			break;
		}

		vec3 Wi = samp.Wi;
		float bsdfPdf = samp.pdf;
		vec3 bsdf = samp.bsdf;
		uint flag = samp.flag;

		bool deltaBsdf = (flag == SpecRefl || flag == SpecTrans);

		if (bsdfPdf < 1e-8) break;
		vec3 lastBeta = beta;
		beta *= bsdf / bsdfPdf * (deltaBsdf ? 1.0 : absDot(N, Wi));

		Ray newRay;
		newRay.ori = P + Wi * 1e-4;
		newRay.dir = Wi;

		SceneHitInfo scHitInfo = sceneHit(newRay);
		int primId = scHitInfo.primId;
		float dist = scHitInfo.dist;
		vec3 hitPoint = rayPoint(newRay, dist);

		if (primId == -1 || primId >= uObjPrimCount)
		{
			int lightId = primId - uObjPrimCount;
			vec3 radiance = (primId == -1) ? envGetRadiance(Wi) : lightGetRadiance(lightId, hitPoint, -Wi);
			float weight = 1.0;

			if (uSampleLight)
			{
				if (!deltaBsdf)
				{
					if (primId == -1)
					{
						float envPdf = envPdfLi(Wi) * pdfSelectEnv();
						if (envPdf <= 0.0) weight = 0.0;
						else weight = biHeuristic(bsdfPdf, envPdf);
					}
					else
					{
						float lightPdf = lightPdfLi(lightId, P, hitPoint) * pdfSelectLight(lightId);
						if (lightPdf <= 0.0) weight = 0.0;
						else weight = biHeuristic(bsdfPdf, lightPdf);
					}
				}
				result += radiance * beta * weight;
			}
			else
			{
				result += radiance * beta;
			}
			break;
		}

		if (flag == SpecTrans || flag == GlosTrans)
			etaScale *= square(samp.eta);

		surfaceInfo = triangleSurfaceInfo(primId, hitPoint);
		id = primId;
		newRay.ori = hitPoint;
		ray = newRay;
	}
	return result;
}

vec3 traceAO(Ray ray, int id, SurfaceInfo surfaceInfo, inout Sampler s)
{
	vec3 ao = vec3(0.0);
	vec3 P = ray.ori;
	vec3 N = surfaceInfo.norm;
	if (dot(N, ray.dir) > 0) N = -N;

	for (int i = 0; i < uMaxBounce; i++)
	{
		vec3 Wi = sampleCosineWeighted(N, sample2D(s)).xyz;

		Ray occRay;
		occRay.ori = P + Wi * 1e-4;
		occRay.dir = Wi;

		float tmp = uAoCoef.x;
		if (bvhTest(occRay, tmp)) ao += 1.0;
	}
	
	return 1.0 - ao / float(uMaxBounce);
}

Ray sampleLensCamera(vec2 uv, inout Sampler s)
{
	vec2 texelSize = 1.0 / vec2(uFrameSize);
	vec2 biasedCoord = uv + texelSize * sample2D(s);
	vec2 ndc = biasedCoord * 2.0 - 1.0;

	vec3 pLens = vec3(toConcentricDisk(sample2D(s)) * uLensRadius, 0.0);
	vec3 pFocusPlane = vec3(ndc * vec2(uCamAsp, 1.0) * uFocalDist * uTanFOV, uFocalDist);
	vec3 dir = pFocusPlane - pLens;
	dir = normalize(uCamR * dir.x + uCamU * dir.y + uCamF * dir.z);

	Ray ret;
	ret.ori = uCamPos + uCamR * pLens.x + uCamU * pLens.y;
	ret.dir = dir;

	return ret;
}

vec3 getResult(Ray ray, inout Sampler s)
{
	vec3 result = vec3(0.0);
	SceneHitInfo scHitInfo = sceneHit(ray);
	int primId = scHitInfo.primId;
	float dist = scHitInfo.dist;
	vec3 hitPoint = rayPoint(ray, dist);

	if (uShowBVH)
	{
		int matId = texelFetch(uMatTexIndices, primId).r & 0xffff;
		if (matId == uMatIndex && primId != -1) result = vec3(1.0, 1.0, 0.2);
		else result = vec3(maxDepth / uBvhDepth * 0.2);
	}
	else
	{
		if (scHitInfo.primId == -1)
		{
			result = envGetRadiance(ray.dir);
		}
		else if (scHitInfo.primId < uObjPrimCount)
		{
			ray.ori = hitPoint;
			SurfaceInfo sInfo = triangleSurfaceInfo(primId, ray.ori);
			result = uAoMode ? traceAO(ray, primId, sInfo, s) : trace(ray, primId, sInfo, s);
		}
		else
		{
			result = lightGetRadiance(primId - uObjPrimCount, hitPoint, -ray.dir);
		}
	}
	return result;
}