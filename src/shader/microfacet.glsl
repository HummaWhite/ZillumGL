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

float smithG(vec3 n, vec3 wo, vec3 wi, float alpha)
{
	return schlickG(absDot(n, wo), alpha) * schlickG(absDot(n, wi), alpha);
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

float ggxD(vec3 n, vec3 m, float alpha)
{
	return ggx(dot(n, m), alpha);
}

float ggxPdfWm(vec3 n, vec3 m, vec3 wo, float alpha)
{
	return ggx(dot(n, m), alpha);
}

float ggxPdfVisibleWm(vec3 n, vec3 m, vec3 wo, float alpha)
{
	return ggx(dot(n, m), alpha) * schlickG(dot(n, wo), alpha) * absDot(m, wo) / absDot(n, wo);
}

vec3 ggxSampleWm(vec3 n, vec3 wo, float alpha, vec2 u)
{
	vec2 xi = toConcentricDisk(u);

	vec3 h = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	h = normalize(h * vec3(alpha, alpha, 1.0));
	return normalToWorld(n, h);
}

vec3 ggxSampleWm(vec3 n, vec3 wo, vec2 alpha, vec2 u)
{
	vec2 xi = toConcentricDisk(u);
	vec3 h = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	h = normalize(h * vec3(alpha, 1.0));
	return normalToWorld(n, h);
}

vec3 ggxSampleVisibleWm(vec3 n, vec3 wo, float alpha, vec2 u)
{
	mat3 tbn = tbnMatrix(n);
	mat3 tbnInv = inverse(tbn);

	vec3 vh = normalize((tbnInv * wo) * vec3(alpha, alpha, 1.0));

	float lensq = vh.x * vh.x + vh.y * vh.y;
	vec3 t1 = lensq > 0.0 ? vec3(-vh.y, vh.x, 0.0) / sqrt(lensq) : vec3(1.0, 0.0, 0.0);
	vec3 t2 = cross(vh, t1);

	vec2 xi = toConcentricDisk(u);
	float s = 0.5 * (1.0 + vh.z);
	xi.y = (1.0 - s) * sqrt(1.0 - xi.x * xi.x) + s * xi.y;

	vec3 h = t1 * xi.x + t2 * xi.y + vh * sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y));
	h = normalize(vec3(h.x * alpha, h.y * alpha, max(0.0, h.z)));
	return normalToWorld(n, h);
}

float gtr1(float cosTheta, float alpha)
{
	float a2 = alpha * alpha;
	return (a2 - 1.0) / (2.0 * Pi * log(alpha) * (1.0 + (a2 - 1.0) * cosTheta * cosTheta));
}

float gtr1D(vec3 n, vec3 m, float alpha)
{
	return gtr1(satDot(n, m), alpha);
}

vec3 gtr1SampleWm(vec3 n, vec3 wo, float alpha, vec2 u)
{
	float cosTheta = sqrt(max(0.0, (1.0 - pow(alpha, 1.0 - u.x)) / (1.0 - alpha)));
	float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
	float phi = 2.0 * u.y * Pi;

	vec3 m = normalize(vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta));
	if (!sameHemisphere(n, wo, m))
		m = -m;
	return normalize(normalToWorld(n, m));
}

float gtr1PdfWm(vec3 n, vec3 m, vec3 wo, float alpha)
{
	return gtr1D(n, m, alpha) * absDot(n, m);
}