@type lib
@include math.shader

const uint Diffuse = 1 << 0;
const uint GlosRefl = 1 << 1;
const uint GlosTrans = 1 << 2;
const uint SpecRefl = 1 << 3;
const uint SpecTrans = 1 << 4;
const uint Invalid = 1 << 16;

const uint MetalWorkflow = 0;
const uint Dielectric = 1;
const uint ThinDielectric = 2;

uniform samplerBuffer materials;
uniform isamplerBuffer matTypes;

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

vec3 schlickF(float cosTheta, vec3 F0, float roughness)
{
	float pow5 = 1.0 - cosTheta;
	pow5 *= pow5;
	pow5 *= pow5 * (1.0 - cosTheta);
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow5;
}

float schlickG(float cosTheta, float alpha)
{
	float k = alpha * 0.5;
	return cosTheta / (cosTheta * (1.0 - k) + k);
}

float smithG(vec3 N, vec3 Wo, vec3 Wi, float alpha)
{
	return schlickG(absDot(N, Wo), alpha) * schlickG(absDot(N, Wi), alpha);
}

float ggx(float cosTheta, float alpha)
{
	if (cosTheta < 1e-6) return 0.0;

	float a2 = alpha * alpha;
	float nom = a2;
	float denom = cosTheta * cosTheta * (a2 - 1.0) + 1.0;
	denom = denom * denom * Pi;

	return nom / denom;
}

float ggxD(vec3 N, vec3 M, float alpha)
{
	return ggx(dot(N, M), alpha);
}

float ggxPdfWm(vec3 N, vec3 M, vec3 Wo, float alpha)
{
	return ggx(dot(N, M), alpha);
}

float ggxPdfVisibleWm(vec3 N, vec3 M, vec3 Wo, float alpha)
{
	return ggx(dot(N, M), alpha) * schlickG(dot(N, Wo), alpha) * absDot(M, Wo) / absDot(N, Wo);
}

vec3 ggxSampleWm(vec3 N, vec3 Wo, float alpha, vec2 u)
{
	vec2 xi = toConcentricDisk(u);

	vec3 H = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	H = normalize(H * vec3(alpha, alpha, 1.0));
	return normalToWorld(N, H);
}

vec3 ggxSampleWm(vec3 N, vec3 Wo, vec2 alpha, vec2 u)
{
	vec2 xi = toConcentricDisk(u);
	vec3 H = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	H = normalize(H * vec3(alpha, 1.0));
	return normalToWorld(N, H);
}

vec3 ggxSampleVisibleWm(vec3 N, vec3 Wo, float alpha, vec2 u)
{
	mat3 tbn = tbnMatrix(N);
	mat3 tbnInv = inverse(tbn);

	vec3 Vh = normalize((tbnInv * Wo) * vec3(alpha, alpha, 1.0));

	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) / sqrt(lensq) : vec3(1.0, 0.0, 0.0);
	vec3 T2 = cross(Vh, T1);

	vec2 xi = toConcentricDisk(u);
	float s = 0.5 * (1.0 + Vh.z);
	xi.y = (1.0 - s) * sqrt(1.0 - xi.x * xi.x) + s * xi.y;

	vec3 H = T1 * xi.x + T2 * xi.y + Vh * sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y));
	H = normalize(vec3(H.x * alpha, H.y * alpha, max(0.0, H.z)));
	return normalToWorld(N, H);
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
	return mix(pdfDiff, pdfSpec, 1.0f / (2.0f - metallic));
}

BsdfSample metalWorkflowGetSample(vec3 N, vec3 Wo, vec3 albedo, float metallic, float roughness, float u, vec2 u2)
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

	vec3 H = normalize(Wi + Wo);
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

BsdfSample dielectricGetSample(vec3 N, vec3 Wo, vec3 tint, float ior, float roughness, float u, vec2 u2)
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

BsdfSample thinDielectricGetSample(vec3 N, vec3 Wo, vec3 tint, float ior, float u, vec2 u2)
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