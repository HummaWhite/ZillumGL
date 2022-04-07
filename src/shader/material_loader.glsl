@type lib

BSDFType loadMaterialType(int matId)
{
	return texelFetch(uMatTypes, matId * 4 + 3).y;
}

vec3 loadBaseColor(int matId, int texId, vec2 uv)
{
	if (texId == -1)
		return texelFetch(uMaterials, matId * 4).rgb;
	else
	{
		vec2 uvScale = texelFetch(uTexUVScale, texId).xy;
		return texture2DArray(uTextures, vec3(fract(uv) * uvScale, texId)).rgb;
	}
}

BSDFParam loadLambertian(int matId, int texId, vec2 uv)
{
	BSDFParam ret;
	ret.baseColor = loadBaseColor(matId, texId, uv);
	return ret;
}

BSDFParam loadMetalWorkflow(int matId, int texId, vec2 uv)
{
	BSDFParam ret;
	vec4 baseRou = texelFetch(uMaterials, matId * 4 + 0);
	ret.baseColor = (texId == -1) ? baseRou.xyz :
		texture2DArray(uTextures, vec3(fract(uv) * texelFetch(uTexUVScale, texId).xy, texId)).rgb;
	ret.roughness = mix(0.0134, 1.0, baseRou.w);
	ret.metallic = texelFetch(uMaterials, matId * 4 + 1).y;
	return ret;
}

BSDFParam loadPrincipledBRDF(int matId, int texId, vec2 uv)
{
	BSDFParam ret;
	vec4 baseRou = texelFetch(uMaterials, matId * 4 + 0);
	vec4 ssMetSpecTint = texelFetch(uMaterials, matId * 4 + 1);
	vec4 sheenTintCoatGloss = texelFetch(uMaterials, matId * 4 + 2);

	ret.baseColor = (texId == -1) ? baseRou.xyz :
		texture2DArray(uTextures, vec3(fract(uv) * texelFetch(uTexUVScale, texId).xy, texId)).rgb;
	ret.roughness = mix(0.0134, 1.0, baseRou.w);

	ret.subsurface = ssMetSpecTint.x;
	ret.metallic = ssMetSpecTint.y;
	ret.specular = ssMetSpecTint.z;
	ret.specularTint = ssMetSpecTint.w;

	ret.sheen = sheenTintCoatGloss.x;
	ret.sheenTint = sheenTintCoatGloss.y;
	ret.clearcoat = sheenTintCoatGloss.z;
	ret.clearcoatGloss = sheenTintCoatGloss.w;
	return ret;
}

BSDFParam loadDielectric(int matId, int texId, vec2 uv)
{
	BSDFParam ret;
	vec4 baseRou = texelFetch(uMaterials, matId * 4 + 0);
	ret.baseColor = (texId == -1) ? baseRou.xyz :
		texture2DArray(uTextures, vec3(fract(uv) * texelFetch(uTexUVScale, texId).xy, texId)).rgb;
	ret.roughness = baseRou.w;
	ret.ior = texelFetch(uMaterials, matId * 4 + 3).x;
	return ret;
}

BSDFParam loadThinDielectric(int matId, int texId, vec2 uv)
{
	BSDFParam ret;
	vec4 baseRou = texelFetch(uMaterials, matId * 4 + 0);
	ret.baseColor = (texId == -1) ? baseRou.xyz :
		texture2DArray(uTextures, vec3(fract(uv) * texelFetch(uTexUVScale, texId).xy, texId)).rgb;
	ret.ior = texelFetch(uMaterials, matId * 4 + 3).x;
	return ret;
}

BSDFParam loadMaterial(uint matType, int matId, int texId, vec2 uv)
{
	switch (matType)
	{
	case Lambertian:
		return loadLambertian(matId, texId, uv);
	case PrincipledBRDF:
		return loadPrincipledBRDF(matId, texId, uv);
	case MetalWorkflow:
		return loadMetalWorkflow(matId, texId, uv);
	case Dielectric:
		return loadDielectric(matId, texId, uv);
	case ThinDielectric:
		return loadThinDielectric(matId, texId, uv);
	}
	return loadLambertian(matId, texId, uv);
}

vec3 materialBSDF(uint matType, BSDFParam param, vec3 wo, vec3 wi, vec3 n, TransportMode mode)
{
	switch (matType)
	{
	case Lambertian:
		return lambertian(wo, wi, n, param, mode);
	case PrincipledBRDF:
		return principledBRDF(wo, wi, n, param, mode);
	case MetalWorkflow:
		return metalWorkflow(wo, wi, n, param, mode);
	case Dielectric:
		return dielectric(wo, wi, n, param, mode);
	case ThinDielectric:
		return vec3(0.0);
	}
	return lambertian(wo, wi, n, param, mode);
}

vec4 materialBSDFAndPdf(uint matType, BSDFParam param, vec3 wo, vec3 wi, vec3 n, TransportMode mode)
{
	switch (matType)
	{
	case Lambertian:
		return vec4(lambertian(wo, wi, n, param, mode), lambertianPdf(wo, wi, n, param, mode));
	case PrincipledBRDF:
		return vec4(principledBRDF(wo, wi, n, param, mode), principledBRDFPdf(wo, wi, n, param, mode));
	case MetalWorkflow:
		return vec4(metalWorkflow(wo, wi, n, param, mode), metalWorkflowPdf(wo, wi, n, param, mode));
	case Dielectric:
		return vec4(dielectric(wo, wi, n, param, mode), dielectricPdf(wo, wi, n, param, mode));
	case ThinDielectric:
		return vec4(0.0);
	}
	return vec4(lambertian(wo, wi, n, param, mode), lambertianPdf(wo, wi, n, param, mode));
}

float materialPdf(uint matType, BSDFParam param, vec3 wo, vec3 wi, vec3 n, TransportMode mode)
{
	switch (matType)
	{
	case Lambertian:
		return lambertianPdf(wo, wi, n, param, mode);
	case PrincipledBRDF:
		return principledBRDFPdf(wo, wi, n, param, mode);
	case MetalWorkflow:
		return metalWorkflowPdf(wo, wi, n, param, mode);
	case Dielectric:
		return dielectricPdf(wo, wi, n, param, mode);
	case ThinDielectric:
		return 0.0;
	}
	return lambertianPdf(wo, wi, n, param, mode);
}

BSDFSample materialSample(uint matType, BSDFParam param, vec3 n, vec3 wo, TransportMode mode, vec3 u)
{
	switch (matType)
	{
	case Lambertian:
		return lambertianSample(n, wo, param, mode, u);
	case PrincipledBRDF:
		return principledBRDFSample(n, wo, param, mode, u);
	case MetalWorkflow:
		return metalWorkflowSample(n, wo, param, mode, u);
	case Dielectric:
		return dielectricSample(n, wo, param, mode, u);
	case ThinDielectric:
		return thinDielectricSample(n, wo, param, mode, u);
	}
	return lambertianSample(n, wo, param, mode, u);
}