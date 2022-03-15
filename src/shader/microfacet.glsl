@type lib
@include math.glsl

float schlickW(float cosTheta)
{
	return pow5(1.0f - cosTheta);
}

vec3 schlickF(float cosTheta, vec3 F0)
{
	return F0 + (vec3(1.0f) - F0) * pow5(1.0f - cosTheta);
}

vec3 schlickF(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow5(1.0 - cosTheta);
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

float gtr1(float cosTheta, float alpha)
{
	float a2 = alpha * alpha;
	return (a2 - 1.0) / (2.0 * Pi * log(alpha) * (1.0 + (a2 - 1.0) * cosTheta * cosTheta));
}

float gtr1D(vec3 N, vec3 M, float alpha)
{
	return gtr1(satDot(N, M), alpha);
}

vec3 gtr1SampleWm(vec3 N, vec3 Wo, float alpha, vec2 u)
{
	float cosTheta = sqrt(max(0.0, (1.0 - pow(alpha, 1.0 - u.x)) / (1.0 - alpha)));
	float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
	float phi = 2.0 * u.y * Pi;

	vec3 M = normalize(vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta));
	if (!sameHemisphere(N, Wo, M))
		M = -M;
	return normalize(normalToWorld(N, M));
}

float gtr1PdfWm(vec3 N, vec3 M, vec3 Wo, float alpha)
{
	return gtr1D(N, M, alpha) * absDot(N, M);
}