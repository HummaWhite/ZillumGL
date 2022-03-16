@type lib
@include microfacet.glsl

const uint Diffuse = 1 << 0;
const uint GlosRefl = 1 << 1;
const uint GlosTrans = 1 << 2;
const uint SpecRefl = 1 << 3;
const uint SpecTrans = 1 << 4;
const uint Invalid = 1 << 16;

const uint Principled = 0;
const uint MetalWorkflow = 1;
const uint Dielectric = 2;
const uint ThinDielectric = 3;

struct MetalWorkflowBRDF
{
	vec3 baseColor;
	float metallic;
	float roughness;
};

struct DielectricBSDF
{
	vec3 baseColor;
	float ior;
	float roughness;
};

struct ThinDielectricBSDF
{
	vec3 baseColor;
	float ior;
};

struct PrincipledBRDF
{
	vec3 baseColor;
	float subsurface;

	float metallic;
	float roughness;
	float specular;
	float specularTint;

	float sheen;
	float sheenTint;
	float clearcoat;
	float clearcoatGloss;
};

struct BsdfSample
{
	vec3 wi;
	float pdf;
	vec3 bsdf;
	float eta;
	uint flag;
};

BsdfSample makeBsdfSample(vec3 wi, float pdf, vec3 bsdf, float eta, uint flag)
{
	BsdfSample samp;
	samp.wi = wi;
	samp.pdf = pdf;
	samp.bsdf = bsdf;
	samp.eta = eta;
	samp.flag = flag;
	return samp;
}

const BsdfSample InvalidBsdfSample = makeBsdfSample(vec3(0), 0, vec3(0), 0, Invalid);

bool approximateDelta(float roughness)
{
	return roughness < 0.02;
}

vec3 metalWorkflowBsdf(vec3 wo, vec3 wi, vec3 n, vec3 albedo, float metallic, float roughness)
{
	float alpha = roughness * roughness;
	vec3 h = normalize(wi + wo);

	if (!sameHemisphere(n, wo, wi)) return vec3(0.0);

	float NdotL = dot(n, wi);
	float NdotV = dot(n, wo);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3	F = schlickF(satDot(h, wo), F0, roughness);
	float	D = ggxD(n, h, alpha);
	float	G = smithG(n, wo, wi, alpha);

	vec3 ks = F;
	vec3 kd = vec3(1.0) - ks;
	kd *= 1.0 - metallic;

	vec3 FDG = F * D * G;
	float denom = 4.0 * NdotV * NdotL;
	if (denom < 1e-7) return vec3(0.0);

	vec3 glossy = FDG / denom;

	return kd * albedo * PiInv + glossy;
}

float metalWorkflowPdf(vec3 wo, vec3 wi, vec3 n, float metallic, float alpha)
{
	vec3 h = normalize(wo + wi);
	float pdfDiff = satDot(n, wi) * PiInv;
	float pdfSpec = ggxPdfVisibleWm(n, h, wo, alpha) / (4.0 * absDot(h, wo));
	float spec = 1.0f / (2.0f - metallic);
	return mix(pdfDiff, pdfSpec, spec);
}

BsdfSample metalWorkflowSample(vec3 n, vec3 wo, vec3 albedo, float metallic, float roughness, float u, vec2 u2)
{
	float spec = 1.0 / (2.0 - metallic);
	uint type = u > spec ? Diffuse : GlosRefl;
	float alpha = square(roughness);

	vec3 wi;
	if (type == Diffuse) wi = sampleCosineWeighted(n, u2).xyz;
	else
	{
		vec3 h = ggxSampleVisibleWm(n, wo, alpha, u2);
		wi = reflect(-wo, h);
	}

	float cosWi = dot(n, wi);
	if (cosWi < 0) return InvalidBsdfSample;

	vec3 bsdf = metalWorkflowBsdf(wo, wi, n, albedo, metallic, roughness);
	float pdf = metalWorkflowPdf(wo, wi, n, metallic, alpha);
	return makeBsdfSample(wi, pdf, bsdf, 1.0, type);
}

bool refract(out vec3 Wt, vec3 wi, vec3 n, float eta)
{
	float cosTi = dot(n, wi);
	if (cosTi < 0) eta = 1.0 / eta;
	float sin2Ti = max(0.0, 1.0 - cosTi * cosTi);
	float sin2Tt = sin2Ti / (eta * eta);

	if (sin2Tt >= 1.0) return false;

	float cosTt = sqrt(1.0 - sin2Tt);
	if (cosTi < 0) cosTt = -cosTt;
	Wt = normalize(-wi / eta + n * (cosTi / eta - cosTt));
	return true;
}

float fresnelDielectric(float cosTi, float eta)
{
	cosTi = clamp(cosTi, -1.0, 1.0);
	if (cosTi < 0.0)
	{
		eta = 1.0 / eta;
		cosTi = -cosTi;
	}

	float sinTi = sqrt(1.0 - cosTi * cosTi);
	float sinTt = sinTi / eta;
	if (sinTt >= 1.0) return 1.0;

	float cosTt = sqrt(1.0 - sinTt * sinTt);

	float rPa = (cosTi - eta * cosTt) / (cosTi + eta * cosTt);
	float rPe = (eta * cosTi - cosTt) / (eta * cosTi + cosTt);
	return (rPa * rPa + rPe * rPe) * 0.5;
}

vec3 dielectricBsdf(vec3 wo, vec3 wi, vec3 n, vec3 tint, float ior, float roughness)
{
	if (approximateDelta(roughness)) return vec3(0.0);

	vec3 h = normalize(wo + wi);

	float hCosWo = absDot(h, wo);
	float hCosWi = absDot(h, wi);
	float alpha = roughness * roughness;

	if (sameHemisphere(n, wo, wi))
	{
		float refl = fresnelDielectric(absDot(h, wi), ior);
		return (hCosWo * hCosWi < 1e-7) ? vec3(0.0) : tint * ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) / (4.0 * hCosWo * hCosWi) * refl;
	}
	else
	{
		float eta = dot(n, wi) > 0 ? ior : 1.0f / ior;
		float sqrtDenom = dot(h, wo) + eta * dot(h, wi);
		float denom = sqrtDenom * sqrtDenom;
		float dHdWi = hCosWi / denom;

		denom *= absDot(n, wi) * absDot(n, wo);
		float refl = fresnelDielectric(dot(h, wi), eta);
		float factor = square(1.0 / eta);
		factor = 1.0;

		return (denom < 1e-7) ?
			vec3(0.0) :
			tint * abs(ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) * hCosWo * hCosWi) / denom * (1.0 - refl) * factor;
	}
}

float dielectricPdf(vec3 wo, vec3 wi, vec3 n, float ior, float roughness)
{
	if (approximateDelta(roughness)) return 0.0;

	if (sameHemisphere(n, wo, wi))
	{
		vec3 h = normalize(wo + wi);
		if (dot(wo, h) < 0.0) return 0.0;

		float refl = fresnelDielectric(absDot(h, wi), ior);
		return ggxPdfWm(n, h, wo, roughness * roughness) / (4.0 * absDot(h, wo)) * refl;
	}
	else
	{
		float eta = dot(n, wo) > 0 ? ior : 1.0 / ior;
		vec3 h = normalize(wo + wi * eta);
		if (sameHemisphere(h, wo, wi)) return 0.0;

		float trans = 1.0f - fresnelDielectric(absDot(h, wo), eta);
		float dHdWi = absDot(h, wi) / square(dot(h, wo) + eta * dot(h, wi));
		return ggxPdfWm(n, h, wo, roughness * roughness) * dHdWi * trans;
	}
}

BsdfSample dielectricSample(vec3 n, vec3 wo, vec3 tint, float ior, float roughness, float u, vec2 u2)
{
	if (approximateDelta(roughness))
	{
		float refl = fresnelDielectric(dot(n, wo), ior), trans = 1.0 - refl;

		if (u < refl)
		{
			vec3 wi = reflect(-wo, n);
			return makeBsdfSample(wi, 1.0, tint, 1.0, SpecRefl);
		}
		else
		{
			vec3 wi;
			bool refr = refract(wi, wo, n, ior);
			if (!refr) return InvalidBsdfSample;
			if (dot(n, wo) < 0) ior = 1.0 / ior;
			float factor = square(1.0 / ior);
			factor = 1.0;

			return makeBsdfSample(wi, 1.0, tint * factor, ior, SpecTrans);
		}
	}
	else
	{
		float alpha = roughness * roughness;
		vec3 h = ggxSampleWm(n, wo, alpha, u2);
		if (dot(n, h) < 0.0f) h = -h;
		float refl = fresnelDielectric(dot(h, wo), ior);
		float trans = 1.0f - refl;

		if (u < refl)
		{
			vec3 wi = -reflect(wo, h);
			if (!sameHemisphere(n, wo, wi)) return InvalidBsdfSample;

			float p = ggxPdfWm(n, h, wo, alpha) / (4.0f * absDot(h, wo));
			float hCosWo = absDot(h, wo);
			float hCosWi = absDot(h, wi);

			vec3 r = (hCosWo * hCosWi < 1e-7) ? vec3(0.0) :
				tint * ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) /
				(4.0 * hCosWo * hCosWi);

			if (isnan(p)) p = 0.0;
			return makeBsdfSample(wi, p, r, 1.0, GlosRefl);
		}
		else
		{
			vec3 wi;
			bool refr = refract(wi, wo, h, ior);
			if (!refr) return InvalidBsdfSample;
			if (sameHemisphere(n, wo, wi)) return InvalidBsdfSample;
			if (absDot(n, wi) < 1e-10) return InvalidBsdfSample;

			float hCosWo = absDot(h, wo);
			float hCosWi = absDot(h, wi);

			if (dot(h, wo) < 0) ior = 1.0 / ior;
			float sqrtDenom = dot(h, wo) + ior * dot(h, wi);
			float denom = sqrtDenom * sqrtDenom;
			float dHdWi = hCosWi / denom;
			float factor = square(1.0 / ior);
			factor = 1.0;

			denom *= absDot(n, wi) * absDot(n, wo);

			vec3 t = (denom < 1e-7) ? vec3(0.0) :
				tint *
				abs(ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) * hCosWo * hCosWi) / denom;

			float p = ggxPdfWm(n, h, wo, alpha) * dHdWi;

			if (isnan(p)) p = 0.0;
			return makeBsdfSample(wi, p, t * factor, ior, GlosTrans);
		}
	}
}

BsdfSample thinDielectricSample(vec3 n, vec3 wo, vec3 tint, float ior, float u, vec2 u2)
{
	if (dot(n, wo) < 0) n = -n;
	float refl = fresnelDielectric(dot(n, wo), ior);
	float trans = 1.0 - refl;
	if (refl < 1.0)
	{
		refl += trans * trans * refl / (1.0 - refl * refl);
		trans = 1.0 - refl;
	}

	if (u < refl)
	{
		vec3 wi = reflect(-wo, n);
		return makeBsdfSample(wi, 1.0, tint, 1.0, SpecRefl);
	}
	return makeBsdfSample(-wo, 1.0, tint, 1.0, SpecTrans);
}

vec3 principledMetalBsdf(vec3 wo, vec3 wi, vec3 n, vec3 Fm0, float alpha)
{
	float cosWo = satDot(n, wo);
	float cosWi = satDot(n, wi);
	vec3 h = normalize(wo + wi);
	if (cosWo < 1e-10 || cosWi < 1e-10)
		return vec3(0.0);

	vec3 Fm = schlickF(absDot(h, wo), Fm0);
	float Dm = ggxD(n, h, alpha);
	float Gm = smithG(n, wo, wi, alpha);

	float denom = 4.0 * cosWi * cosWo;
	if (denom < 1e-7)
		return vec3(0.0);
	return Fm * Dm * Gm / denom;
}

float principledMetalPdf(vec3 wo, vec3 wi, vec3 n, float alpha)
{
	vec3 h = normalize(wo + wi);
	return ggxPdfVisibleWm(n, h, wo, alpha) / (4.0 * absDot(h, wo));
}

BsdfSample principledMetalSample(vec3 n, vec3 wo, vec3 Fm0, float alpha, float u, vec2 u2)
{
	vec3 h = ggxSampleVisibleWm(n, wo, alpha, u2);
	vec3 wi = reflect(-wo, h);

	if (dot(n, wi) < 0.0)
		return InvalidBsdfSample;
	vec3 bsdf = principledMetalBsdf(wo, wi, n, Fm0, alpha);
	float pdf = principledMetalPdf(wo, wi, n, alpha);
	return makeBsdfSample(wi, pdf, bsdf, 1.0, GlosRefl);
}

vec3 principledClearcoatBsdf(vec3 wo, vec3 wi, vec3 n, vec3 baseColor, float alpha)
{
	float cosWo = satDot(n, wo);
	float cosWi = satDot(n, wi);
	vec3 h = normalize(wo + wi);
	if (cosWo < 1e-6 || cosWi < 1e-6)
		return vec3(0.0);

	vec3 Fc = schlickF(absDot(h, wo), baseColor);
	float Dc = gtr1D(n, h, alpha);
	float Gc = smithG(n, wo, wi, 0.25);

	float denom = 4.0 * cosWi * cosWo;
	if (denom < 1e-7)
		return vec3(0.0);
	return Fc * Dc * Gc / denom;
}

float principledClearcoatPdf(vec3 wo, vec3 wi, vec3 n, float alpha)
{
	vec3 h = normalize(wo + wi);
	return gtr1PdfWm(n, h, wo, alpha) / (4.0 * absDot(h, wo));
}

BsdfSample principledClearcoatSample(vec3 n, vec3 wo, vec3 baseColor, float alpha, float u, vec2 u2)
{
	vec3 h = gtr1SampleWm(n, wo, alpha, u2);
	vec3 wi = reflect(-wo, h);
	
	if (dot(n, wi) < 0.0)
		return InvalidBsdfSample;
	vec3 bsdf = principledClearcoatBsdf(wo, wi, n, baseColor, alpha);
	float pdf = principledClearcoatPdf(wo, wi, n, alpha);
	return makeBsdfSample(wi, pdf, bsdf, 1.0, GlosRefl);
}

vec3 principledDiffuseBsdf(vec3 wo, vec3 wi, vec3 n, vec3 baseColor, float subsurface, float roughness)
{
	float cosWo = satDot(n, wo);
	float cosWi = satDot(n, wi);
	if (cosWo < 1e-10 || cosWi < 1e-10)
		return vec3(0.0);

	vec3 h = normalize(wo + wi);
	float hCosWi = dot(h, wi);
	float HoWi2 = hCosWi * hCosWi;

	float Fi = schlickW(cosWi);
	float Fo = schlickW(cosWo);

	vec3 Fd90 = vec3(0.5 + 2.0 * roughness * HoWi2);
	vec3 Fd = mix(vec3(1.0), Fd90, Fi) * mix(vec3(1.0), Fd90, Fo);
	vec3 baseDiffuse = baseColor * Fd * PiInv;

	vec3 Fss90 = vec3(roughness * HoWi2);
	vec3 Fss = mix(vec3(1.0), Fss90, Fi) * mix(vec3(1.0), Fss90, Fo);
	vec3 ss = baseColor * PiInv * 1.25 * (Fss * (1.0 / (cosWi + cosWo) - 0.5) + 0.5);

	return mix(baseDiffuse, ss, subsurface);
}

BsdfSample principledDiffuseSample(vec3 n, vec3 wo, vec3 baseColor, float subsurface, float roughness, float u, vec2 u2)
{
	vec4 samp = sampleCosineWeighted(n, u2);
	vec3 wi = samp.xyz;
	vec3 bsdf = principledDiffuseBsdf(wo, wi, n, baseColor, subsurface, roughness);
	return makeBsdfSample(wi, samp.w, bsdf, 1.0, Diffuse);
}

vec3 principledBsdf(vec3 wo, vec3 wi, vec3 n, PrincipledBRDF param)
{
	vec3 res = vec3(0.0);

	vec3 baseColor = param.baseColor;
	float subsurface = param.subsurface;
	float metallic = param.metallic;
	float roughness = param.roughness;
	float specular = param.specular;
	float specularTint = param.specularTint;
	float sheen = param.sheen;
	float sheenTint = param.sheenTint;
	float clearcoat = param.clearcoat;
	float clearcoatGloss = param.clearcoatGloss;
	
	float alpha = square(roughness);
	float clearcoatAlpha = mix(0.1, 0.001, clearcoatGloss);
	float lum = luminance(baseColor);
	vec3 tintColor = lum > 0 ? baseColor / lum : vec3(1.0);
	vec3 Fm0 = mix(0.08 * specular * mix(vec3(1.0), tintColor, specularTint), baseColor, metallic);
	float hCosWi = dot(normalize(wo + wi), wi);

	res += principledDiffuseBsdf(wo, wi, n, baseColor, subsurface, roughness) * (1.0 - metallic);
	res += principledMetalBsdf(wo, wi, n, Fm0, alpha);
	res += principledClearcoatBsdf(wo, wi, n, baseColor, clearcoatAlpha) * clearcoat * 0.25;
	res += mix(vec3(1.0), tintColor, sheenTint) * schlickW(hCosWi) * sheen * (dot(n, wi) < 0.0 ? 0.0 : 1.0);

	return res;
}

float principledPdf(vec3 wo, vec3 wi, vec3 n, PrincipledBRDF param)
{
	float pdf = 0.0;
	vec3 baseColor = param.baseColor;
	float subsurface = param.subsurface;
	float metallic = param.metallic;
	float roughness = param.roughness;
	float specular = param.specular;
	float specularTint = param.specularTint;
	float sheen = param.sheen;
	float sheenTint = param.sheenTint;
	float clearcoat = param.clearcoat;
	float clearcoatGloss = param.clearcoatGloss;

	float alpha = square(roughness);
	float clearcoatAlpha = mix(0.1, 0.001, clearcoatGloss);
	float spec = 1.0 / (2.0 - metallic);
	float cosinePdf = absDot(n, wi) * PiInv;
	pdf += cosinePdf * (1.0 - spec);
	pdf += principledMetalPdf(wo, wi, n, alpha) * spec;
	pdf += principledClearcoatPdf(wo, wi, n, clearcoatAlpha) * 0.25 * clearcoat;
	return pdf / (1.0 + 0.25 * clearcoat);
}

BsdfSample principledSample(vec3 n, vec3 wo, PrincipledBRDF param, float u, vec2 u2)
{
	vec3 baseColor = param.baseColor;
	float subsurface = param.subsurface;
	float metallic = param.metallic;
	float roughness = param.roughness;
	float specular = param.specular;
	float specularTint = param.specularTint;
	float sheen = param.sheen;
	float sheenTint = param.sheenTint;
	float clearcoat = param.clearcoat;
	float clearcoatGloss = param.clearcoatGloss;

	float alpha = square(roughness);
	float clearcoatAlpha = mix(0.1, 0.001, clearcoatGloss);
	float spec = 1.0 / (2.0 - metallic);

	vec3 wi;
	float cdf[3];
	cdf[0] = 1.0 - spec;
	cdf[1] = 1.0;
	cdf[2] = 1.0 + clearcoat * 0.25;

	float s = rand() * cdf[2];
	if (s <= cdf[0])
		wi = principledDiffuseSample(n, wo, baseColor, subsurface, roughness, u, u2).wi;
	else if (s <= cdf[1])
	{
		float lum = luminance(baseColor);
		vec3 tintColor = lum > 0 ? baseColor / lum : vec3(1.0);
		vec3 Fm0 = mix(0.08 * specular * mix(vec3(1.0), tintColor, specularTint), baseColor, metallic);
		wi = principledMetalSample(n, wo, Fm0, alpha, u, u2).wi;
	}
	else
		wi = principledClearcoatSample(n, wo, baseColor, alpha, u, u2).wi;

	vec3 bsdf = principledBsdf(wo, wi, n, param);
	float pdf = principledPdf(wo, wi, n, param);
	return makeBsdfSample(wi, pdf, bsdf, 1.0, Diffuse);
}

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