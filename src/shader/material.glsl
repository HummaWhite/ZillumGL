@type lib
@include microfacet.glsl

#define BSDFFlag uint
const BSDFFlag Diffuse = 1 << 0;
const BSDFFlag GlosRefl = 1 << 1;
const BSDFFlag GlosTrans = 1 << 2;
const BSDFFlag SpecRefl = 1 << 3;
const BSDFFlag SpecTrans = 1 << 4;
const BSDFFlag Invalid = 1 << 16;

#define BSDFType uint
const BSDFType Lambertian = 0;
const BSDFType PrincipledBRDF = 1;
const BSDFType MetalWorkflow = 2;
const BSDFType Dielectric = 3;
const BSDFType ThinDielectric = 4;

#define TransportMode uint
const TransportMode Radiance = 0;
const TransportMode Importance = 1;

struct BSDFParam
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

	float ior;
};

struct BSDFSample
{
	vec3 wi;
	float pdf;
	vec3 bsdf;
	float eta;
	uint flag;
};

BSDFSample makeBSDFSample(vec3 wi, float pdf, vec3 bsdf, float eta, BSDFFlag flag)
{
	BSDFSample samp;
	samp.wi = wi;
	samp.pdf = pdf;
	samp.bsdf = bsdf;
	samp.eta = eta;
	samp.flag = flag;
	return samp;
}

const BSDFSample InvalidBSDFSample = makeBSDFSample(vec3(0), 0, vec3(0), 0, Invalid);

bool approximateDelta(float roughness)
{
	return roughness < 0.02;
}

vec3 lambertian(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
{
	return param.baseColor * PiInv;
}

float lambertianPdf(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
{
	return satDot(wi, n) * PiInv;
}

BSDFSample lambertianSample(vec3 n, vec3 wo, BSDFParam param, TransportMode mode, vec3 u)
{
	vec3 wi = sampleCosineWeighted(n, u.yz).xyz;
	float pdf = satDot(n, wi) * PiInv;
	return makeBSDFSample(wi, pdf, param.baseColor * PiInv, 1.0, Diffuse);
}

vec3 metalWorkflow(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
{
	vec3 baseColor = param.baseColor;
	float metallic = param.metallic;
	float roughness = param.roughness;
	float alpha = square(roughness);
	vec3 h = normalize(wi + wo);

	if (!sameHemisphere(n, wo, wi))
		return vec3(0.0);

	float cosWi = dot(n, wi);
	float cosWo = dot(n, wo);

	vec3 f0 = mix(vec3(0.04), baseColor, metallic);

	vec3 f = schlickF(satDot(h, wo), f0, roughness);
	float d = ggxD(n, h, alpha);
	float g = smithG(n, wo, wi, alpha);

	vec3 ks = f;
	vec3 kd = vec3(1.0) - ks;
	kd *= 1.0 - metallic;

	float denom = 4.0 * cosWo * cosWi;
	if (denom < 1e-7)
		return vec3(0.0);

	return kd * baseColor * PiInv + f * d * g / denom;
}

float metalWorkflowPdf(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
{
	float alpha = square(param.roughness);
	vec3 h = normalize(wo + wi);
	float pdfDiff = satDot(n, wi) * PiInv;
	float pdfSpec = ggxPdfVisibleWm(n, h, wo, alpha) / (4.0 * absDot(h, wo));
	float spec = 1.0f / (2.0f - param.metallic);
	return mix(pdfDiff, pdfSpec, spec);
}

BSDFSample metalWorkflowSample(vec3 n, vec3 wo, BSDFParam param, TransportMode mode, vec3 u)
{
	float roughness = param.roughness;
	float alpha = square(roughness);

	float spec = 1.0 / (2.0 - param.metallic);
	uint type = u.x > spec ? Diffuse : GlosRefl;

	vec3 wi;
	if (type == Diffuse) wi = sampleCosineWeighted(n, u.yz).xyz;
	else
	{
		vec3 h = ggxSampleVisibleWm(n, wo, alpha, u.yz);
		wi = reflect(-wo, h);
	}

	float cosWi = dot(n, wi);
	if (cosWi < 0)
		return InvalidBSDFSample;

	vec3 bsdf = metalWorkflow(wo, wi, n, param, mode);
	float pdf = metalWorkflowPdf(wo, wi, n, param, mode);
	return makeBSDFSample(wi, pdf, bsdf, 1.0, type);
}

bool refract(out vec3 wt, vec3 wi, vec3 n, float eta)
{
	float cosTi = dot(n, wi);
	if (cosTi < 0) eta = 1.0 / eta;
	float sin2Ti = max(0.0, 1.0 - cosTi * cosTi);
	float sin2Tt = sin2Ti / (eta * eta);

	if (sin2Tt >= 1.0)
		return false;

	float cosTt = sqrt(1.0 - sin2Tt);
	if (cosTi < 0) cosTt = -cosTt;
	wt = normalize(-wi / eta + n * (cosTi / eta - cosTt));
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
	if (sinTt >= 1.0)
		return 1.0;

	float cosTt = sqrt(1.0 - sinTt * sinTt);

	float rPa = (cosTi - eta * cosTt) / (cosTi + eta * cosTt);
	float rPe = (eta * cosTi - cosTt) / (eta * cosTi + cosTt);
	return (rPa * rPa + rPe * rPe) * 0.5;
}

vec3 dielectric(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
{
	vec3 baseColor = param.baseColor;
	float roughness = param.roughness;
	float ior = param.ior;

	if (approximateDelta(roughness))
		return vec3(0.0);

	vec3 h = normalize(wo + wi);

	float hCosWo = absDot(h, wo);
	float hCosWi = absDot(h, wi);
	float alpha = roughness * roughness;

	if (sameHemisphere(n, wo, wi))
	{
		float refl = fresnelDielectric(absDot(h, wi), ior);
		return (hCosWo * hCosWi < 1e-7) ? vec3(0.0) : baseColor * ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) / (4.0 * hCosWo * hCosWi) * refl;
	}
	else
	{
		float eta = dot(n, wi) > 0 ? ior : 1.0f / ior;
		float sqrtDenom = dot(h, wo) + eta * dot(h, wi);
		float denom = sqrtDenom * sqrtDenom;
		float dHdWi = hCosWi / denom;

		denom *= absDot(n, wi) * absDot(n, wo);
		float refl = fresnelDielectric(dot(h, wi), eta);
		float factor = (mode == Radiance) ? square(1.0 / eta) : 1.0;

		return (denom < 1e-7) ?
			vec3(0.0) :
			baseColor * abs(ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) * hCosWo * hCosWi) / denom * (1.0 - refl) * factor;
	}
}

float dielectricPdf(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
{
	float roughness = param.roughness;
	float ior = param.ior;
	if (approximateDelta(roughness))
		return 0.0;

	if (sameHemisphere(n, wo, wi))
	{
		vec3 h = normalize(wo + wi);
		if (dot(wo, h) < 0.0)
			return 0.0;

		float refl = fresnelDielectric(absDot(h, wi), ior);
		return ggxPdfWm(n, h, wo, roughness * roughness) / (4.0 * absDot(h, wo)) * refl;
	}
	else
	{
		float eta = dot(n, wo) > 0 ? ior : 1.0 / ior;
		vec3 h = normalize(wo + wi * eta);
		if (sameHemisphere(h, wo, wi))
			return 0.0;

		float trans = 1.0 - fresnelDielectric(absDot(h, wo), eta);
		float dHdWi = absDot(h, wi) / square(dot(h, wo) + eta * dot(h, wi));
		return ggxPdfWm(n, h, wo, roughness * roughness) * dHdWi * trans;
	}
}

BSDFSample dielectricSample(vec3 n, vec3 wo, BSDFParam param, TransportMode mode, vec3 u)
{
	vec3 baseColor = param.baseColor;
	float roughness = param.roughness;
	float ior = param.ior;

	if (approximateDelta(roughness))
	{
		float refl = fresnelDielectric(dot(n, wo), ior);
		float trans = 1.0 - refl;

		if (u.x < refl)
		{
			vec3 wi = reflect(-wo, n);
			return makeBSDFSample(wi, 1.0, baseColor, 1.0, SpecRefl);
		}
		else
		{
			vec3 wi;
			bool refr = refract(wi, wo, n, ior);
			if (!refr)
				return InvalidBSDFSample;

			if (dot(n, wo) < 0)
				ior = 1.0 / ior;
			float factor = (mode == Radiance) ? square(1.0 / ior) : 1.0;
			return makeBSDFSample(wi, 1.0, baseColor * factor, ior, SpecTrans);
		}
	}
	else
	{
		float alpha = roughness * roughness;
		vec3 h = ggxSampleWm(n, wo, alpha, u.yz);
		if (dot(n, h) < 0.0f) h = -h;
		float refl = fresnelDielectric(dot(h, wo), ior);
		float trans = 1.0f - refl;

		if (u.x < refl)
		{
			vec3 wi = -reflect(wo, h);
			if (!sameHemisphere(n, wo, wi))
				return InvalidBSDFSample;

			float p = ggxPdfWm(n, h, wo, alpha) / (4.0f * absDot(h, wo));
			float hCosWo = absDot(h, wo);
			float hCosWi = absDot(h, wi);

			vec3 r = (hCosWo * hCosWi < 1e-7) ? vec3(0.0) :
				baseColor * ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) /
				(4.0 * hCosWo * hCosWi);

			if (isnan(p)) p = 0.0;
			return makeBSDFSample(wi, p, r, 1.0, GlosRefl);
		}
		else
		{
			vec3 wi;
			bool refr = refract(wi, wo, h, ior);
			if (!refr) return InvalidBSDFSample;
			if (sameHemisphere(n, wo, wi)) return InvalidBSDFSample;
			if (absDot(n, wi) < 1e-10) return InvalidBSDFSample;

			float hCosWo = absDot(h, wo);
			float hCosWi = absDot(h, wi);

			if (dot(h, wo) < 0)
				ior = 1.0 / ior;
			float sqrtDenom = dot(h, wo) + ior * dot(h, wi);
			float denom = sqrtDenom * sqrtDenom;
			float dHdWi = hCosWi / denom;
			float factor = (mode == Radiance) ? square(1.0 / ior) : 1.0;

			denom *= absDot(n, wi) * absDot(n, wo);

			vec3 t = (denom < 1e-7) ? vec3(0.0) :
				baseColor *
				abs(ggxD(n, h, alpha) * smithG(n, wo, wi, alpha) * hCosWo * hCosWi) / denom;

			float p = ggxPdfWm(n, h, wo, alpha) * dHdWi;

			if (isnan(p)) p = 0.0;
			return makeBSDFSample(wi, p, t * factor, ior, GlosTrans);
		}
	}
}

BSDFSample thinDielectricSample(vec3 n, vec3 wo, BSDFParam param, TransportMode mode, vec3 u)
{
	if (dot(n, wo) < 0)
		n = -n;
	float refl = fresnelDielectric(dot(n, wo), param.ior);
	float trans = 1.0 - refl;
	if (refl < 1.0)
	{
		refl += trans * trans * refl / (1.0 - refl * refl);
		trans = 1.0 - refl;
	}

	return (u.x < refl) ?
		makeBSDFSample(reflect(-wo, n), 1.0, param.baseColor, 1.0, SpecRefl) :
		makeBSDFSample(-wo, 1.0, param.baseColor, 1.0, SpecTrans);
}

vec3 principledMetal(vec3 wo, vec3 wi, vec3 n, vec3 fm0, float alpha)
{
	float cosWo = satDot(n, wo);
	float cosWi = satDot(n, wi);
	vec3 h = normalize(wo + wi);
	if (cosWo < 1e-10 || cosWi < 1e-10)
		return vec3(0.0);

	vec3 fm = schlickF(absDot(h, wo), fm0);
	float dm = ggxD(n, h, alpha);
	float gm = smithG(n, wo, wi, alpha);

	float denom = 4.0 * cosWi * cosWo;
	if (denom < 1e-7)
		return vec3(0.0);
	return fm * dm * gm / denom;
}

float principledMetalPdf(vec3 wo, vec3 wi, vec3 n, float alpha)
{
	vec3 h = normalize(wo + wi);
	return ggxPdfVisibleWm(n, h, wo, alpha) / (4.0 * absDot(h, wo));
}

BSDFSample principledMetalSample(vec3 n, vec3 wo, vec3 fm0, float alpha, vec3 u)
{
	vec3 h = ggxSampleVisibleWm(n, wo, alpha, u.yz);
	vec3 wi = reflect(-wo, h);

	if (dot(n, wi) < 0.0)
		return InvalidBSDFSample;
	vec3 bsdf = principledMetal(wo, wi, n, fm0, alpha);
	float pdf = principledMetalPdf(wo, wi, n, alpha);
	return makeBSDFSample(wi, pdf, bsdf, 1.0, GlosRefl);
}

vec3 principledClearcoat(vec3 wo, vec3 wi, vec3 n, vec3 baseColor, float alpha)
{
	float cosWo = satDot(n, wo);
	float cosWi = satDot(n, wi);
	vec3 h = normalize(wo + wi);
	if (cosWo < 1e-6 || cosWi < 1e-6)
		return vec3(0.0);

	vec3 fc = schlickF(absDot(h, wo), baseColor);
	float dc = gtr1D(n, h, alpha);
	float gc = smithG(n, wo, wi, 0.25);

	float denom = 4.0 * cosWi * cosWo;
	if (denom < 1e-7)
		return vec3(0.0);
	return fc * dc * gc / denom;
}

float principledClearcoatPdf(vec3 wo, vec3 wi, vec3 n, float alpha)
{
	vec3 h = normalize(wo + wi);
	return gtr1PdfWm(n, h, wo, alpha) / (4.0 * absDot(h, wo));
}

BSDFSample principledClearcoatSample(vec3 n, vec3 wo, vec3 baseColor, float alpha, vec3 u)
{
	vec3 h = gtr1SampleWm(n, wo, alpha, u.yz);
	vec3 wi = reflect(-wo, h);
	
	if (dot(n, wi) < 0.0)
		return InvalidBSDFSample;
	vec3 bsdf = principledClearcoat(wo, wi, n, baseColor, alpha);
	float pdf = principledClearcoatPdf(wo, wi, n, alpha);
	return makeBSDFSample(wi, pdf, bsdf, 1.0, GlosRefl);
}

vec3 principledDiffuse(vec3 wo, vec3 wi, vec3 n, vec3 baseColor, float subsurface, float roughness)
{
	float cosWo = satDot(n, wo);
	float cosWi = satDot(n, wi);
	if (cosWo < 1e-10 || cosWi < 1e-10)
		return vec3(0.0);

	vec3 h = normalize(wo + wi);
	float hCosWi = dot(h, wi);
	float hCosWi2 = hCosWi * hCosWi;

	float fi = schlickW(cosWi);
	float fo = schlickW(cosWo);

	vec3 fd90 = vec3(0.5 + 2.0 * roughness * hCosWi2);
	vec3 fd = mix(vec3(1.0), fd90, fi) * mix(vec3(1.0), fd90, fo);
	vec3 baseDiffuse = baseColor * fd * PiInv;

	vec3 fss90 = vec3(roughness * hCosWi2);
	vec3 fss = mix(vec3(1.0), fss90, fi) * mix(vec3(1.0), fss90, fo);
	vec3 ss = baseColor * PiInv * 1.25 * (fss * (1.0 / (cosWi + cosWo) - 0.5) + 0.5);

	return mix(baseDiffuse, ss, subsurface);
}

BSDFSample principledDiffuseSample(vec3 n, vec3 wo, vec3 baseColor, float subsurface, float roughness, vec3 u)
{
	vec4 samp = sampleCosineWeighted(n, u.yz);
	vec3 wi = samp.xyz;
	vec3 bsdf = principledDiffuse(wo, wi, n, baseColor, subsurface, roughness);
	return makeBSDFSample(wi, samp.w, bsdf, 1.0, Diffuse);
}

vec3 principledBRDF(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
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
	vec3 fm0 = mix(0.08 * specular * mix(vec3(1.0), tintColor, specularTint), baseColor, metallic);
	float hCosWi = dot(normalize(wo + wi), wi);

	res += principledDiffuse(wo, wi, n, baseColor, subsurface, roughness) * (1.0 - metallic);
	res += principledMetal(wo, wi, n, fm0, alpha);
	res += principledClearcoat(wo, wi, n, baseColor, clearcoatAlpha) * clearcoat * 0.25;
	res += mix(vec3(1.0), tintColor, sheenTint) * schlickW(hCosWi) * sheen * (dot(n, wi) < 0.0 ? 0.0 : 1.0);

	return res;
}

float principledBRDFPdf(vec3 wo, vec3 wi, vec3 n, BSDFParam param, TransportMode mode)
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

BSDFSample principledBRDFSample(vec3 n, vec3 wo, BSDFParam param, TransportMode mode, vec3 u)
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
		wi = principledDiffuseSample(n, wo, baseColor, subsurface, roughness, u).wi;
	else if (s <= cdf[1])
	{
		float lum = luminance(baseColor);
		vec3 tintColor = lum > 0 ? baseColor / lum : vec3(1.0);
		vec3 fm0 = mix(0.08 * specular * mix(vec3(1.0), tintColor, specularTint), baseColor, metallic);
		wi = principledMetalSample(n, wo, fm0, alpha, u).wi;
	}
	else
		wi = principledClearcoatSample(n, wo, baseColor, alpha, u).wi;

	vec3 bsdf = principledBRDF(wo, wi, n, param, mode);
	float pdf = principledBRDFPdf(wo, wi, n, param, mode);
	return makeBSDFSample(wi, pdf, bsdf, 1.0, Diffuse);
}