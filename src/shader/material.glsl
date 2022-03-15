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
	vec3 Wi;
	float pdf;
	vec3 bsdf;
	float eta;
	uint flag;
};

BsdfSample bsdfSample(vec3 Wi, float pdf, vec3 bsdf, float eta, uint flag)
{
	BsdfSample samp;
	samp.Wi = Wi;
	samp.pdf = pdf;
	samp.bsdf = bsdf;
	samp.eta = eta;
	samp.flag = flag;
	return samp;
}

const BsdfSample INVALID_SAMPLE = bsdfSample(vec3(0), 0, vec3(0), 0, Invalid);

bool approximateDelta(float roughness)
{
	return roughness < 0.02;
}

vec3 metalWorkflowBsdf(vec3 Wo, vec3 Wi, vec3 N, vec3 albedo, float metallic, float roughness)
{
	float alpha = roughness * roughness;
	vec3 H = normalize(Wi + Wo);

	if (!sameHemisphere(N, Wo, Wi)) return vec3(0.0);

	float NdotL = dot(N, Wi);
	float NdotV = dot(N, Wo);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3	F = schlickF(satDot(H, Wo), F0, roughness);
	float	D = ggxD(N, H, alpha);
	float	G = smithG(N, Wo, Wi, alpha);

	vec3 ks = F;
	vec3 kd = vec3(1.0) - ks;
	kd *= 1.0 - metallic;

	vec3 FDG = F * D * G;
	float denom = 4.0 * NdotV * NdotL;
	if (denom < 1e-7) return vec3(0.0);

	vec3 glossy = FDG / denom;

	return kd * albedo * PiInv + glossy;
}

float metalWorkflowPdf(vec3 Wo, vec3 Wi, vec3 N, float metallic, float alpha)
{
	vec3 H = normalize(Wo + Wi);
	float pdfDiff = satDot(N, Wi) * PiInv;
	float pdfSpec = ggxPdfVisibleWm(N, H, Wo, alpha) / (4.0 * absDot(H, Wo));
	float spec = 1.0f / (2.0f - metallic);
	return mix(pdfDiff, pdfSpec, spec);
}

BsdfSample metalWorkflowSample(vec3 N, vec3 Wo, vec3 albedo, float metallic, float roughness, float u, vec2 u2)
{
	float spec = 1.0 / (2.0 - metallic);
	uint type = u > spec ? Diffuse : GlosRefl;
	float alpha = square(roughness);

	vec3 Wi;
	if (type == Diffuse) Wi = sampleCosineWeighted(N, u2).xyz;
	else
	{
		vec3 H = ggxSampleVisibleWm(N, Wo, alpha, u2);
		Wi = reflect(-Wo, H);
	}

	float NoWi = dot(N, Wi);
	if (NoWi < 0) return INVALID_SAMPLE;

	vec3 bsdf = metalWorkflowBsdf(Wo, Wi, N, albedo, metallic, roughness);
	float pdf = metalWorkflowPdf(Wo, Wi, N, metallic, alpha);
	return bsdfSample(Wi, pdf, bsdf, 1.0, type);
}

bool refract(out vec3 Wt, vec3 Wi, vec3 N, float eta)
{
	float cosTi = dot(N, Wi);
	if (cosTi < 0) eta = 1.0 / eta;
	float sin2Ti = max(0.0, 1.0 - cosTi * cosTi);
	float sin2Tt = sin2Ti / (eta * eta);

	if (sin2Tt >= 1.0) return false;

	float cosTt = sqrt(1.0 - sin2Tt);
	if (cosTi < 0) cosTt = -cosTt;
	Wt = normalize(-Wi / eta + N * (cosTi / eta - cosTt));
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

vec3 dielectricBsdf(vec3 Wo, vec3 Wi, vec3 N, vec3 tint, float ior, float roughness)
{
	if (approximateDelta(roughness)) return vec3(0.0);

	vec3 H = normalize(Wo + Wi);

	float HoWo = absDot(H, Wo);
	float HoWi = absDot(H, Wi);
	float alpha = roughness * roughness;

	if (sameHemisphere(N, Wo, Wi))
	{
		float refl = fresnelDielectric(absDot(H, Wi), ior);
		return (HoWo * HoWi < 1e-7) ? vec3(0.0) : tint * ggxD(N, H, alpha) * smithG(N, Wo, Wi, alpha) / (4.0 * HoWo * HoWi) * refl;
	}
	else
	{
		float eta = dot(N, Wi) > 0 ? ior : 1.0f / ior;
		float sqrtDenom = dot(H, Wo) + eta * dot(H, Wi);
		float denom = sqrtDenom * sqrtDenom;
		float dHdWi = HoWi / denom;

		denom *= absDot(N, Wi) * absDot(N, Wo);
		float refl = fresnelDielectric(dot(H, Wi), eta);
		float factor = square(1.0 / eta);
		factor = 1.0;

		return (denom < 1e-7) ?
			vec3(0.0) :
			tint * abs(ggxD(N, H, alpha) * smithG(N, Wo, Wi, alpha) * HoWo * HoWi) / denom * (1.0 - refl) * factor;
	}
}

float dielectricPdf(vec3 Wo, vec3 Wi, vec3 N, float ior, float roughness)
{
	if (approximateDelta(roughness)) return 0.0;

	if (sameHemisphere(N, Wo, Wi))
	{
		vec3 H = normalize(Wo + Wi);
		if (dot(Wo, H) < 0.0) return 0.0;

		float refl = fresnelDielectric(absDot(H, Wi), ior);
		return ggxPdfWm(N, H, Wo, roughness * roughness) / (4.0 * absDot(H, Wo)) * refl;
	}
	else
	{
		float eta = dot(N, Wo) > 0 ? ior : 1.0 / ior;
		vec3 H = normalize(Wo + Wi * eta);
		if (sameHemisphere(H, Wo, Wi)) return 0.0;

		float trans = 1.0f - fresnelDielectric(absDot(H, Wo), eta);
		float dHdWi = absDot(H, Wi) / square(dot(H, Wo) + eta * dot(H, Wi));
		return ggxPdfWm(N, H, Wo, roughness * roughness) * dHdWi * trans;
	}
}

BsdfSample dielectricSample(vec3 N, vec3 Wo, vec3 tint, float ior, float roughness, float u, vec2 u2)
{
	if (approximateDelta(roughness))
	{
		float refl = fresnelDielectric(dot(N, Wo), ior), trans = 1.0 - refl;

		if (u < refl)
		{
			vec3 Wi = reflect(-Wo, N);
			return bsdfSample(Wi, 1.0, tint, 1.0, SpecRefl);
		}
		else
		{
			vec3 Wi;
			bool refr = refract(Wi, Wo, N, ior);
			if (!refr) return INVALID_SAMPLE;
			if (dot(N, Wo) < 0) ior = 1.0 / ior;
			float factor = square(1.0 / ior);
			factor = 1.0;

			return bsdfSample(Wi, 1.0, tint * factor, ior, SpecTrans);
		}
	}
	else
	{
		float alpha = roughness * roughness;
		vec3 H = ggxSampleWm(N, Wo, alpha, u2);
		if (dot(N, H) < 0.0f) H = -H;
		float refl = fresnelDielectric(dot(H, Wo), ior);
		float trans = 1.0f - refl;

		if (u < refl)
		{
			vec3 Wi = -reflect(Wo, H);
			if (!sameHemisphere(N, Wo, Wi)) return INVALID_SAMPLE;

			float p = ggxPdfWm(N, H, Wo, alpha) / (4.0f * absDot(H, Wo));
			float HoWo = absDot(H, Wo);
			float HoWi = absDot(H, Wi);

			vec3 r = (HoWo * HoWi < 1e-7) ? vec3(0.0) :
				tint * ggxD(N, H, alpha) * smithG(N, Wo, Wi, alpha) /
				(4.0 * HoWo * HoWi);

			if (isnan(p)) p = 0.0;
			return bsdfSample(Wi, p, r, 1.0, GlosRefl);
		}
		else
		{
			vec3 Wi;
			bool refr = refract(Wi, Wo, H, ior);
			if (!refr) return INVALID_SAMPLE;
			if (sameHemisphere(N, Wo, Wi)) return INVALID_SAMPLE;
			if (absDot(N, Wi) < 1e-10) return INVALID_SAMPLE;

			float HoWo = absDot(H, Wo);
			float HoWi = absDot(H, Wi);

			if (dot(H, Wo) < 0) ior = 1.0 / ior;
			float sqrtDenom = dot(H, Wo) + ior * dot(H, Wi);
			float denom = sqrtDenom * sqrtDenom;
			float dHdWi = HoWi / denom;
			float factor = square(1.0 / ior);
			factor = 1.0;

			denom *= absDot(N, Wi) * absDot(N, Wo);

			vec3 t = (denom < 1e-7) ? vec3(0.0) :
				tint *
				abs(ggxD(N, H, alpha) * smithG(N, Wo, Wi, alpha) * HoWo * HoWi) / denom;

			float p = ggxPdfWm(N, H, Wo, alpha) * dHdWi;

			if (isnan(p)) p = 0.0;
			return bsdfSample(Wi, p, t * factor, ior, GlosTrans);
		}
	}
}

BsdfSample thinDielectricSample(vec3 N, vec3 Wo, vec3 tint, float ior, float u, vec2 u2)
{
	if (dot(N, Wo) < 0) N = -N;
	float refl = fresnelDielectric(dot(N, Wo), ior);
	float trans = 1.0 - refl;
	if (refl < 1.0)
	{
		refl += trans * trans * refl / (1.0 - refl * refl);
		trans = 1.0 - refl;
	}

	if (u < refl)
	{
		vec3 Wi = reflect(-Wo, N);
		return bsdfSample(Wi, 1.0, tint, 1.0, SpecRefl);
	}
	return bsdfSample(-Wo, 1.0, tint, 1.0, SpecTrans);
}

vec3 principledMetalBsdf(vec3 Wo, vec3 Wi, vec3 N, vec3 Fm0, float alpha)
{
	float NoWo = satDot(N, Wo);
	float NoWi = satDot(N, Wi);
	vec3 H = normalize(Wo + Wi);
	if (NoWo < 1e-10 || NoWi < 1e-10)
		return vec3(0.0);

	vec3 Fm = schlickF(absDot(H, Wo), Fm0);
	float Dm = ggxD(N, H, alpha);
	float Gm = smithG(N, Wo, Wi, alpha);

	float denom = 4.0 * NoWi * NoWo;
	if (denom < 1e-7)
		return vec3(0.0);
	return Fm * Dm * Gm / denom;
}

float principledMetalPdf(vec3 Wo, vec3 Wi, vec3 N, float alpha)
{
	vec3 H = normalize(Wo + Wi);
	return ggxPdfVisibleWm(N, H, Wo, alpha) / (4.0 * absDot(H, Wo));
}

BsdfSample principledMetalSample(vec3 N, vec3 Wo, vec3 Fm0, float alpha, float u, vec2 u2)
{
	vec3 H = ggxSampleVisibleWm(N, Wo, alpha, u2);
	vec3 Wi = reflect(-Wo, H);

	if (dot(N, Wi) < 0.0)
		return INVALID_SAMPLE;
	vec3 bsdf = principledMetalBsdf(Wo, Wi, N, Fm0, alpha);
	float pdf = principledMetalPdf(Wo, Wi, N, alpha);
	return bsdfSample(Wi, pdf, bsdf, 1.0, GlosRefl);
}

vec3 principledClearcoatBsdf(vec3 Wo, vec3 Wi, vec3 N, vec3 baseColor, float alpha)
{
	float NoWo = satDot(N, Wo);
	float NoWi = satDot(N, Wi);
	vec3 H = normalize(Wo + Wi);
	if (NoWo < 1e-6 || NoWi < 1e-6)
		return vec3(0.0);

	vec3 Fc = schlickF(absDot(H, Wo), baseColor);
	float Dc = gtr1D(N, H, alpha);
	float Gc = smithG(N, Wo, Wi, 0.25);

	float denom = 4.0 * NoWi * NoWo;
	if (denom < 1e-7)
		return vec3(0.0);
	return Fc * Dc * Gc / denom;
}

float principledClearcoatPdf(vec3 Wo, vec3 Wi, vec3 N, float alpha)
{
	vec3 H = normalize(Wo + Wi);
	return gtr1PdfWm(N, H, Wo, alpha) / (4.0 * absDot(H, Wo));
}

BsdfSample principledClearcoatSample(vec3 N, vec3 Wo, vec3 baseColor, float alpha, float u, vec2 u2)
{
	vec3 H = gtr1SampleWm(N, Wo, alpha, u2);
	vec3 Wi = reflect(-Wo, H);
	
	if (dot(N, Wi) < 0.0)
		return INVALID_SAMPLE;
	vec3 bsdf = principledClearcoatBsdf(Wo, Wi, N, baseColor, alpha);
	float pdf = principledClearcoatPdf(Wo, Wi, N, alpha);
	return bsdfSample(Wi, pdf, bsdf, 1.0, GlosRefl);
}

vec3 principledDiffuseBsdf(vec3 Wo, vec3 Wi, vec3 N, vec3 baseColor, float subsurface, float roughness)
{
	float NoWo = satDot(N, Wo);
	float NoWi = satDot(N, Wi);
	if (NoWo < 1e-10 || NoWi < 1e-10)
		return vec3(0.0);

	vec3 H = normalize(Wo + Wi);
	float HoWi = dot(H, Wi);
	float HoWi2 = HoWi * HoWi;

	float Fi = schlickW(NoWi);
	float Fo = schlickW(NoWo);

	vec3 Fd90 = vec3(0.5 + 2.0 * roughness * HoWi2);
	vec3 Fd = mix(vec3(1.0), Fd90, Fi) * mix(vec3(1.0), Fd90, Fo);
	vec3 baseDiffuse = baseColor * Fd * PiInv;

	vec3 Fss90 = vec3(roughness * HoWi2);
	vec3 Fss = mix(vec3(1.0), Fss90, Fi) * mix(vec3(1.0), Fss90, Fo);
	vec3 ss = baseColor * PiInv * 1.25 * (Fss * (1.0 / (NoWi + NoWo) - 0.5) + 0.5);

	return mix(baseDiffuse, ss, subsurface);
}

BsdfSample principledDiffuseSample(vec3 N, vec3 Wo, vec3 baseColor, float subsurface, float roughness, float u, vec2 u2)
{
	vec4 samp = sampleCosineWeighted(N, u2);
	vec3 Wi = samp.xyz;
	vec3 bsdf = principledDiffuseBsdf(Wo, Wi, N, baseColor, subsurface, roughness);
	return bsdfSample(Wi, samp.w, bsdf, 1.0, Diffuse);
}

vec3 principledBsdf(vec3 Wo, vec3 Wi, vec3 N, PrincipledBRDF param)
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
	float HoWi = dot(normalize(Wo + Wi), Wi);

	res += principledDiffuseBsdf(Wo, Wi, N, baseColor, subsurface, roughness) * (1.0 - metallic);
	res += principledMetalBsdf(Wo, Wi, N, Fm0, alpha);
	res += principledClearcoatBsdf(Wo, Wi, N, baseColor, clearcoatAlpha) * clearcoat * 0.25;
	res += mix(vec3(1.0), tintColor, sheenTint) * schlickW(HoWi) * sheen * (dot(N, Wi) < 0.0 ? 0.0 : 1.0);

	return res;
}

float principledPdf(vec3 Wo, vec3 Wi, vec3 N, PrincipledBRDF param)
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
	float cosinePdf = absDot(N, Wi) * PiInv;
	pdf += cosinePdf * (1.0 - spec);
	pdf += principledMetalPdf(Wo, Wi, N, alpha) * spec;
	pdf += principledClearcoatPdf(Wo, Wi, N, clearcoatAlpha) * 0.25 * clearcoat;
	return pdf / (1.0 + 0.25 * clearcoat);
}

BsdfSample principledSample(vec3 N, vec3 Wo, PrincipledBRDF param, float u, vec2 u2)
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

	vec3 Wi;
	float cdf[3];
	cdf[0] = 1.0 - spec;
	cdf[1] = 1.0;
	cdf[2] = 1.0 + clearcoat * 0.25;

	float s = rand() * cdf[2];
	if (s <= cdf[0])
		Wi = principledDiffuseSample(N, Wo, baseColor, subsurface, roughness, u, u2).Wi;
	else if (s <= cdf[1])
	{
		float lum = luminance(baseColor);
		vec3 tintColor = lum > 0 ? baseColor / lum : vec3(1.0);
		vec3 Fm0 = mix(0.08 * specular * mix(vec3(1.0), tintColor, specularTint), baseColor, metallic);
		Wi = principledMetalSample(N, Wo, Fm0, alpha, u, u2).Wi;
	}
	else
		Wi = principledClearcoatSample(N, Wo, baseColor, alpha, u, u2).Wi;

	vec3 bsdf = principledBsdf(Wo, Wi, N, param);
	float pdf = principledPdf(Wo, Wi, N, param);
	return bsdfSample(Wi, pdf, bsdf, 1.0, Diffuse);
}