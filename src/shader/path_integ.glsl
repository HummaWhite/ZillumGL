@type lib

@include random.glsl
@include math.glsl
@include material.glsl
@include intersection.glsl
@include env_light.glsl
@include light.glsl

uniform bool uSampleLight;
uniform bool uRussianRoulette;

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

vec3 pathIntegTrace(Ray ray, int id, SurfaceInfo surfaceInfo, inout Sampler s)
{
	vec3 result = vec3(0.0);
	vec3 beta = vec3(1.0);
	float etaScale = 1.0;

	for (int bounce = 0; bounce <= uMaxBounce; bounce++)
	{
		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;

		//if (dot(N, Wo) < 0) N = -N;
		int matTexId = texelFetch(uMatTexIndices, id).r;
		int matId = matTexId & 0x0000ffff;
		int texId = matTexId >> 16;

		PrincipledBRDF matParam = loadPrincipledBRDF(matId, texId, surfaceInfo.texCoord);
		
		/*MetalWorkflowBRDF matParam = loadMetalWorkflow(matId, texId, surfaceInfo.texCoord);
		vec3 baseColor = matParam.baseColor;
		float metallic = matParam.metallic;
		float roughness = matParam.roughness;*/

		if (uSampleLight)
		{
			float ud = sample1D(s);
			vec4 us = sample4D(s);

			LightSample samp = sampleLightAndEnv(P, ud, us);
			if (samp.pdf > 0.0)
			{
				float bsdfPdf = principledPdf(Wo, samp.Wi, N, matParam);
				//float bsdfPdf = metalWorkflowPdf(Wo, samp.Wi, N, metallic, roughness * roughness);
				float weight = biHeuristic(samp.pdf, bsdfPdf);
				result +=
					principledBsdf(Wo, samp.Wi, N, matParam)
					//metalWorkflowBsdf(Wo, samp.Wi, N, baseColor, metallic, roughness)
					* beta * satDot(N, samp.Wi) * samp.coef * weight;
			}
		}

		float uf = sample1D(s);
		vec2 u = sample2D(s);
		BsdfSample samp;
		samp = principledSample(N, Wo, matParam, uf, u);
		//samp = metalWorkflowSample(N, Wo, baseColor, metallic, roughness, uf, u);

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

		float dist;
		int primId = bvhHit(newRay, dist);
		int lightId = primId - uObjPrimCount;
		vec3 hitPoint = rayPoint(newRay, dist);
		
		if (primId == -1)
		{
			vec3 radiance = envGetRadiance(Wi);
			float weight = 1.0;
			if (uSampleLight && !deltaBsdf)
			{
				float envPdf = envPdfLi(Wi) * pdfSelectEnv();
				weight = (envPdf <= 0.0) ? 0.0 : biHeuristic(bsdfPdf, envPdf);
			}
			result += radiance * beta * weight;
			break;
		}
		else if (lightId >= 0)
		{
			vec3 radiance = lightGetRadiance(lightId, hitPoint, -Wi);
			float weight = 1.0;
			if (uSampleLight && !deltaBsdf)
			{
				float lightPdf = lightPdfLi(lightId, P, hitPoint) * pdfSelectLight(lightId);
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

		surfaceInfo = triangleSurfaceInfo(primId, hitPoint);
		id = primId;
		newRay.ori = hitPoint;
		ray = newRay;
	}
	return result;
}