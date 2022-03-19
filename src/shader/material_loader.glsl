@type lib

BSDFType loadMaterialType(int matId)
{
	return texelFetch(uMatTypes, matId * 4 + 3).w;
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
	ret.baseColor = loadBaseColor(matId, texId, uv);
	vec2 metRough = texelFetch(uMaterials, matId * 4 + 1).xy;
	ret.metallic = mix(0.0134, 1.0, metRough.x);
	ret.roughness = metRough.y;
	return ret;
}

BSDFParam loadPrincipledBRDF(int matId, int texId, vec2 uv)
{
	BSDFParam ret;
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