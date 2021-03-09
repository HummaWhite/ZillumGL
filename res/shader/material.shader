=type lib
=include math.shader

const uint Diffuse = 1 << 0;
const uint GlosRefl = 1 << 1;
const uint GlosTrans = 1 << 2;
const uint SpecRefl = 1 << 3;
const uint SpecTrans = 1 << 4;
const uint Invalid = 1 << 16;

struct BsdfSample
{
	vec3 Wi;
	float pdf;
	vec3 bsdf;
	uint flag;
};

BsdfSample bsdfSample(vec3 Wi, float pdf, vec3 bsdf, uint flag)
{
	BsdfSample samp;
	samp.Wi = Wi;
	samp.pdf = pdf;
	samp.bsdf = bsdf;
	samp.flag = flag;
	return samp;
}

const BsdfSample INVALID_SAMPLE = bsdfSample(vec3(0), 0, vec3(0), Invalid);

vec2 getAlpha(float roughness, float aniso)
{
	float r2 = roughness * roughness;
	float al = sqrt(1.0 - aniso * 0.9);
	return vec2(r2 / al, r2 * al);
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
	float NoWi = absDot(N, Wo);
	float NoWo = absDot(N, Wi);

	float g2 = schlickG(NoWi, alpha);
	float g1 = schlickG(NoWo, alpha);

	return g1 * g2;
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

float ggx(float cosTheta, float sinPhi, vec2 alpha)
{
	if (cosTheta < 1e-6) return 0.0;

	float sinPhi2 = sinPhi * sinPhi;

	float p = (1.0 - sinPhi2) / (alpha.x * alpha.x) + sinPhi2 / (alpha.y * alpha.y);
	float k = 1.0 + (p - 1) * (1.0 - cosTheta * cosTheta);
	k = k * k * Pi * alpha.x * alpha.y;

	return 1.0 / k;
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

vec3 ggxSampleWm(vec2 xi, vec3 N, vec3 Wo, float alpha)
{
	xi = toConcentricDisk(xi);

	vec3 H = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	H = normalize(H * vec3(alpha, alpha, 1.0));
	return normalToWorld(N, H);
}

vec3 ggxSampleWm(vec2 xi, vec3 N, vec3 Wo, vec2 alpha)
{
	xi = toConcentricDisk(xi);
	vec3 H = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	H = normalize(H * vec3(alpha, 1.0));
	return normalToWorld(N, H);
}

vec3 ggxSampleVisibleWm(vec2 xi, vec3 N, vec3 Wo, float alpha)
{
	mat3 tbn = tbnMatrix(N);
	mat3 tbnInv = inverse(tbn);

	vec3 Vh = normalize((tbnInv * Wo) * vec3(alpha, alpha, 1.0));

	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) / sqrt(lensq) : vec3(1.0, 0.0, 0.0);
	vec3 T2 = cross(Vh, T1);

	xi = toConcentricDisk(xi);
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

	if (dot(N, Wo) < 0) return vec3(0.0);
	if (dot(N, Wi) < 0) return vec3(0.0);

	float NdotL = satDot(N, Wi);
	float NdotV = satDot(N, Wo);

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
	float pdfDiff = dot(N, Wi) * PiInv;
	float pdfSpec = ggxPdfVisibleWm(N, H, Wo, alpha) / (4.0 * absDot(H, Wo));
	return mix(pdfDiff, pdfSpec, 1.0f / (2.0f - metallic));
}

BsdfSample metalWorkflowGetSample(vec3 N, vec3 Wo, vec3 albedo, float metallic, float roughness)
{
	float spec = 1.0 / (2.0 - metallic);
	bool sampleDiffuse = rand() > spec;
	float alpha = square(roughness);

	vec3 Wi;
	if (sampleDiffuse) Wi = sampleCosineWeighted(N).xyz;
	else
	{
		vec3 H = ggxSampleVisibleWm(randBox(), N, Wo, alpha);
		Wi = reflect(-Wo, H);
	}

	float NoWi = dot(N, Wi);
	if (NoWi < 0) INVALID_SAMPLE;

	vec3 H = normalize(Wi + Wo);
	vec3 bsdf = metalWorkflowBsdf(Wo, Wi, N, albedo, metallic, roughness);
	float pdf = metalWorkflowPdf(Wo, Wi, N, metallic, alpha);

	return bsdfSample(Wi, pdf, bsdf, sampleDiffuse ? Diffuse : GlosRefl);
}