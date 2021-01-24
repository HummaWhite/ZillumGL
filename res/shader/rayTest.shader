//$Vertex
#version 450 core
layout(location = 0) in vec2 pos;

out vec2 scrCoord;

void main()
{
	vec2 ndc = pos * 2.0 - 1.0;
	gl_Position = vec4(ndc, 0.0, 1.0);
	scrCoord = pos;
}

//$Fragment
#version 450 core
in vec2 scrCoord;

out vec4 FragColor;

const float Pi = 3.14159265358979323846;
const float PiInv = 1.0 / Pi;
const float INF = 1.0 / 0.0;

struct Ray
{
	vec3 ori;
	vec3 dir;
};

struct SurfaceInfo
{
	vec3 norm;
};

struct SurfaceInteraction
{
	vec3 Wo;
	vec3 Wi;
	vec3 N;
};

struct HitInfo
{
	bool hit;
	float dist;
};

struct SceneHitInfo
{
	int shapeId;
	float dist;
};

struct Material
{
	vec3 albedo;
	float metallic;
	float roughness;
	float anisotropic;
};

struct Light
{
	vec3 va;
	vec3 vb;
	vec3 vc;
	vec3 power;
};

uniform vec3 camF;
uniform vec3 camR;
uniform vec3 camU;
uniform vec3 camPos;
uniform float tanFOV;
uniform float camAsp;
uniform float camNear;
uniform sampler2D skybox;
uniform sampler2D lastFrame;
uniform int spp;
uniform int freeCounter;
uniform bool showBVH;
uniform int boxIndex;
uniform float bvhDepth;

uniform samplerBuffer vertices;
uniform samplerBuffer normals;
uniform isamplerBuffer indices;
uniform samplerBuffer bounds;
uniform isamplerBuffer sizeIndices;
uniform isamplerBuffer matIndices;

const int MAX_MATERIALS = 16;
uniform Material materials[MAX_MATERIALS];

const int MAX_LIGHTS = 8;
uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

bool BUG = false;
vec3 BUGVAL = vec3(0.0);

uint randSeed;
uint hash(uint seed)
{
	seed = (seed ^ uint(61)) ^ (seed >> uint(16));
	seed *= uint(9);
	seed = seed ^ (seed >> uint(4));
	seed *= uint(0x27d4eb2d);
	seed = seed ^ (seed >> uint(15));
	return seed;
}

float rand()
{
	randSeed = hash(randSeed);
	return float(randSeed) * (1.0 / 4294967296.0);
}

float noise(float x)
{
	float y = fract(sin(x) * 100000.0);
	return y;
}

float noise(vec2 st)
{
	return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float rand(float vMin, float vMax)
{
	return mix(vMin, vMax, rand());
}

vec2 randBox()
{
	return vec2(rand(), rand());
}

vec2 toConcentricDisk(vec2 v)
{
	if (v.x == 0.0 && v.y == 0.0) return vec2(0.0, 0.0);
	v = v * 2.0 - 1.0;
	float phi, r;
	if (v.x * v.x > v.y * v.y)
	{
		r = v.x;
		phi = Pi * v.y / v.x * 0.25;
	}
	else
	{
		r = v.y;
		phi = Pi * 0.5 - Pi * v.x / v.y * 0.25;
	}
	return vec2(r * cos(phi), r * sin(phi));
}

float inverseBits(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint n)
{
	return vec2(float(i) / float(n), inverseBits(i));
}

float satDot(vec3 a, vec3 b)
{
	return max(dot(a, b), 0.0);
}

vec2 sphereToPlane(vec3 uv)
{
	float theta = atan(uv.y, uv.x);
	if (theta < 0.0) theta += Pi * 2.0;
	float phi = atan(length(uv.xy), uv.z);
	return vec2(theta * PiInv * 0.5, phi * PiInv);
}

vec3 planeToSphere(vec2 uv)
{
	float theta = uv.x * Pi * 2.0;
	float phi = uv.y * Pi;
	return vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
}

vec3 getEnvRadiance(vec3 dir)
{
	return texture(skybox, sphereToPlane(dir)).rgb;
}

vec3 rayPoint(Ray ray, float t)
{
	return ray.ori + ray.dir * t;
}

HitInfo intersectTriangle(vec3 a, vec3 b, vec3 c, Ray ray)
{
	HitInfo ret;
	const float eps = 1e-6;

	vec3 ab = b - a;
	vec3 ac = c - a;
	vec3 o = ray.ori;
	vec3 d = ray.dir;

	vec3 p = cross(d, ac);
	float det = dot(ab, p);

	if (abs(det) < eps)
	{
		ret.hit = false;
		return ret;
	}

	vec3 ao = o - a;
	if (det < 0)
	{
		ao = -ao;
		det = -det;
	}

	float u = dot(ao, p);
	if (u < 0.0 || u > det)
	{
		ret.hit = false;
		return ret;
	}

	vec3 q = cross(ao, ab);

	float v = dot(d, q);
	if (v < 0.0 || u + v > det)
	{
		ret.hit = false;
		return ret;
	}

	float t = dot(ac, q) / det;
	ret.hit = (t > 0.0);
	ret.dist = t;
	return ret;
}

HitInfo intersectTriangle(int id, Ray ray)
{
	int ia = texelFetch(indices, id * 3 + 0).r;
	int ib = texelFetch(indices, id * 3 + 1).r;
	int ic = texelFetch(indices, id * 3 + 2).r;

	vec3 a = texelFetch(vertices, ia).xyz;
	vec3 b = texelFetch(vertices, ib).xyz;
	vec3 c = texelFetch(vertices, ic).xyz;
	return intersectTriangle(a, b, c, ray);
}

SurfaceInfo triangleSurfaceInfo(int id, vec3 p)
{
	SurfaceInfo ret;

	int ia = texelFetch(indices, id * 3 + 0).r;
	int ib = texelFetch(indices, id * 3 + 1).r;
	int ic = texelFetch(indices, id * 3 + 2).r;

	vec3 a = texelFetch(vertices, ia).xyz;
	vec3 b = texelFetch(vertices, ib).xyz;
	vec3 c = texelFetch(vertices, ic).xyz;

	vec3 na = texelFetch(normals, ia).xyz;
	vec3 nb = texelFetch(normals, ib).xyz;
	vec3 nc = texelFetch(normals, ic).xyz;

	vec3 pa = a - p;
	vec3 pb = b - p;
	vec3 pc = c - p;

	float areaInv = 1.0 / length(cross(b - a, c - a));
	float la = length(cross(pb, pc)) * areaInv;
	float lb = length(cross(pc, pa)) * areaInv;
	float lc = 1 - la - lb;

	ret.norm = normalize(na * la + nb * lb + nc * lc);

	return ret;
}

bool boxHit(int id, Ray ray, out float tMin)
{
	float tMax;
	vec3 pMin = texelFetch(bounds, id * 2 + 0).xyz;
	vec3 pMax = texelFetch(bounds, id * 2 + 1).xyz;

	const float eps = 1e-6;
	vec3 o = ray.ori;
	vec3 d = ray.dir;

	if (abs(d.x) > 1.0 - eps)
	{
		if (o.y > pMin.y && o.y < pMax.y && o.z > pMin.z && o.z < pMax.z)
		{
			float dxInv = 1.0 / d.x;
			float ta = (pMin.x - o.x) * dxInv;
			float tb = (pMax.x - o.x) * dxInv;
			tMin = min(ta, tb);
			tMax = max(ta, tb);
			return tMax >= 0.0 && tMax >= tMin;
		}
		else return false;
	}

	if (abs(d.y) > 1.0 - eps)
	{
		if (o.x > pMin.x && o.x < pMax.x && o.z > pMin.z && o.z < pMax.z)
		{
			float dyInv = 1.0 / d.y;
			float ta = (pMin.y - o.y) * dyInv;
			float tb = (pMax.y - o.y) * dyInv;
			tMin = min(ta, tb);
			tMax = max(ta, tb);
			return tMax >= 0.0 && tMax >= tMin;
		}
		else return false;
	}

	if (abs(d.z) > 1.0 - eps)
	{
		if (o.x > pMin.x && o.x < pMax.x && o.y > pMin.y && o.y < pMax.y)
		{
			float dzInv = 1.0 / d.z;
			float ta = (pMin.z - o.z) * dzInv;
			float tb = (pMax.z - o.z) * dzInv;
			tMin = min(ta, tb);
			tMax = max(ta, tb);
			return tMax >= 0.0 && tMax >= tMin;
		}
		else return false;
	}

	vec3 dInv = 1.0 / d;
	vec3 vta = (pMin - o) * dInv;
	vec3 vtb = (pMax - o) * dInv;

	vec3 vtMin = min(vta, vtb);
	vec3 vtMax = max(vta, vtb);

	vec3 dt = vtMax - vtMin;

	float tyz = vtMax.z - vtMin.y;
	float tzx = vtMax.x - vtMin.z;
	float txy = vtMax.y - vtMin.x;

	if (abs(d.x) < eps)
	{
		if (dt.y + dt.z > tyz)
		{
			tMin = max(vtMin.y, vtMin.z);
			tMax = min(vtMax.y, vtMax.z);
			return tMax >= 0.0 && tMax >= tMin;
		}
	}

	if (abs(d.y) < eps)
	{
		if (dt.z + dt.x > tzx)
		{
			tMin = max(vtMin.z, vtMin.x);
			tMax = min(vtMax.z, vtMax.x);
			return tMax >= 0.0 && tMax >= tMin;
		}
	}

	if (abs(d.z) < eps)
	{
		if (dt.x + dt.y > txy)
		{
			tMin = max(vtMin.x, vtMin.y);
			tMax = min(vtMax.x, vtMax.y);
			return tMax >= 0.0 && tMax >= tMin;
		}
	}

	if (dt.y + dt.z > tyz && dt.z + dt.x > tzx && dt.x + dt.y > txy)
	{
		tMin = max(max(vtMin.x, vtMin.y), vtMin.z);
		tMax = min(min(vtMax.x, vtMax.y), vtMax.z);
		return tMax >= 0.0 && tMax >= tMin;
	}

	return false;
}

const int STACK_SIZE = 64;
int stack[STACK_SIZE];
float maxDepth = 0.0;

int bvhHit(Ray ray, inout float dist, bool quickCheck)
{
	dist = 1e8;
	int closestHit = -1;

	int top = 0;
	stack[top++] = 0;

	while (top > 0)
	{
		int k = stack[--top];
		float boxDist;

		if (!boxHit(k, ray, boxDist)) continue;
		if (boxDist > dist) continue;

		int sizeIndex = texelFetch(sizeIndices, k).r;
		if (sizeIndex < 0)
		{
			sizeIndex ^= 0x80000000;
			HitInfo hInfo = intersectTriangle(sizeIndex, ray);
			if (hInfo.hit && hInfo.dist <= dist)
			{
				if (quickCheck) return 1;
				dist = hInfo.dist;
				closestHit = sizeIndex;
			}
			maxDepth += 1.0;
			continue;
		}
		int lSize = texelFetch(sizeIndices, k + 1).r;
		if (lSize < 0) lSize = 1;
		stack[top++] = k + 1 + lSize;
		stack[top++] = k + 1;
		maxDepth += 1.0;
	}

	return closestHit;
}

SceneHitInfo sceneHit(Ray ray)
{
	SceneHitInfo ret;
	ret.shapeId = bvhHit(ray, ret.dist, false);

	for (int i = 0; i < lightCount; i++)
	{
		HitInfo h = intersectTriangle(lights[i].va, lights[i].vb, lights[i].vc, ray);
		if (h.hit && h.dist < ret.dist)
		{
			ret.shapeId = -i;
			ret.dist = h.dist;
		}
	}

	return ret;
}

vec3 getTangent(vec3 N)
{
	return (abs(N.z) > 0.999) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
}

mat3 TBNMatrix(vec3 N)
{
	vec3 T = getTangent(N);
	vec3 B = normalize(cross(N, T));
	T = cross(B, N);
	return mat3(T, B, N);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec2 getAlpha(float roughness, float aniso)
{
	float r2 = roughness * roughness;
	float al = sqrt(1.0 - aniso * 0.9);
	return vec2(r2 / al, r2 * al);
}

float geometrySchlick(float cosTheta, float alpha)
{
	float k = alpha * 0.5;

	float nom = cosTheta;
	float denom = cosTheta * (1.0 - k) + k;

	return nom / denom;
}

float geometrySmith(vec3 N, vec3 Wo, vec3 Wi, float alpha)
{
	float NoWi = satDot(N, Wo);
	float NoWo = satDot(N, Wi);

	float ggx2 = geometrySchlick(NoWi, alpha);
	float ggx1 = geometrySchlick(NoWo, alpha);

	return ggx1 * ggx2;
}

vec3 sampleUniformHemisphere(vec3 N)
{
	float theta = rand(0.0, Pi * 2.0);
	float phi = rand(0.0, Pi * 0.5);
	vec3 V = vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
	return normalize(TBNMatrix(N) * V);
}

vec4 sampleCosineWeighted(vec3 N)
{
	vec3 vec = sampleUniformHemisphere(N);
	return vec4(vec, max(dot(vec, N), 0.0) * PiInv);
}

float GGX(float cosTheta, float alpha)
{
	if (cosTheta < 1e-6) return 0.0;

	float a2 = alpha * alpha;

	float nom = a2;
	float denom = cosTheta * cosTheta * (a2 - 1.0) + 1.0;
	denom = denom * denom * Pi;
	return nom / denom;
}

float GGX(float cosTheta, float sinPhi, vec2 alpha)
{
	if (cosTheta < 1e-6) return 0.0;

	float sinPhi2 = sinPhi * sinPhi;

	float p = (1.0 - sinPhi2) / (alpha.x * alpha.x) + sinPhi2 / (alpha.y * alpha.y);
	float k = 1.0 + (p - 1) * (1.0 - cosTheta * cosTheta);
	k = k * k * Pi * alpha.x * alpha.y;

	return 1.0 / k;
}

float GGXVNDF(float cosTheta, float NoWo, float MoWo, float alpha)
{
	return geometrySchlick(NoWo, alpha) * GGX(cosTheta, alpha) * MoWo / NoWo;
}

float pdfGGX(float cosTheta, float MoWo, float alpha)
{
	return GGX(cosTheta, alpha) * cosTheta / (4.0 * MoWo);
}

float pdfGGX(float cosTheta, float sinPhi, float MoWo, vec2 alpha)
{
	return GGX(cosTheta, sinPhi, alpha) * cosTheta / (4.0 * MoWo);
}

float pdfGGXVNDF(vec3 N, vec3 M, vec3 Wo, vec3 Wi, float alpha)
{
	return GGXVNDF(dot(N, M), dot(N, Wo), satDot(N, Wo), alpha) / (4.0 * dot(N, Wo));
}

vec3 sampleGGX(vec2 xi, vec3 N, vec3 Wo, float alpha)
{
	xi = toConcentricDisk(xi);

	vec3 H = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	H = normalize(H * vec3(alpha, alpha, 1.0));
	H = normalize(TBNMatrix(N) * H);

	return reflect(-Wo, H);
}

vec3 sampleGGX(vec2 xi, vec3 N, vec3 Wo, vec2 alpha)
{
	xi = toConcentricDisk(xi);
	vec3 H = vec3(xi.x, xi.y, sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y)));
	H = normalize(H * vec3(alpha, 1.0));
	H = normalize(TBNMatrix(N) * H);

	return reflect(-Wo, H);
}

vec3 sampleGGXVNDF(vec2 xi, vec3 N, vec3 Wo, float alpha)
{
	mat3 TBN = TBNMatrix(N);
	mat3 TBNinv = inverse(TBN);

	vec3 Vh = normalize((TBNinv * Wo) * vec3(alpha, alpha, 1.0));

	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) / sqrt(lensq) : vec3(1.0, 0.0, 0.0);
	vec3 T2 = cross(Vh, T1);

	xi = toConcentricDisk(xi);
	float s = 0.5 * (1.0 + Vh.z);
	xi.y = (1.0 - s) * sqrt(1.0 - xi.x * xi.x) + s * xi.y;

	vec3 H = T1 * xi.x + T2 * xi.y + Vh * sqrt(max(0.0, 1.0 - xi.x * xi.x - xi.y * xi.y));
	H = normalize(vec3(H.x * alpha, H.y * alpha, max(0.0, H.z)));
	H = normalize(TBN * H);

	return reflect(-Wo, H);
}

vec3 bsdf(int matId, vec3 Wo, vec3 Wi, vec3 N)
{
	vec3 albedo = materials[matId].albedo;
	float metallic = materials[matId].metallic;
	float roughness = materials[matId].roughness;
	float alpha = roughness * roughness;

	vec3 H = normalize(Wi + Wo);

	if (dot(N, Wo) < 0) return vec3(0.0);
	if (dot(N, Wi) < 0) return vec3(0.0);

	float NdotL = satDot(N, Wi);
	float NdotV = satDot(N, Wo);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3	F = fresnelSchlick(satDot(H, Wo), F0, roughness);
	float	D = GGX(satDot(N, H), alpha);
	float	G = geometrySmith(N, Wo, Wi, alpha);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	vec3 FDG = F * D * G;
	float denominator = 4.0 * NdotV * NdotL;
	if (denominator < 1e-7) return vec3(0.0);

	vec3 glossy = FDG / denominator;

	return (kD * albedo * PiInv + glossy) * NdotL;
}

vec4 getSample(int matId, vec3 N, vec3 Wo)
{
	vec4 ret;
	float metallic = materials[matId].metallic;
	float roughness = materials[matId].roughness;
	float aniso = materials[matId].anisotropic;
	vec2 alpha = getAlpha(roughness, aniso);

	float spec = 1.0 / (2.0 - metallic);
	bool sampleDiffuse = (rand() > spec);
	vec3 Wi = sampleDiffuse ? sampleCosineWeighted(N).xyz : sampleGGXVNDF(randBox(), N, Wo, roughness * roughness);

	float NoWi = dot(N, Wi);
	if (NoWi < 0) return vec4(0.0);

	ret.xyz = Wi;
	vec3 H = normalize(Wi + Wo);

	float pdfDiff = dot(Wi, N) * PiInv;
	//float pdfSpec = pdfGGX(dot(N, H), dot(H, Wo), roughness * roughness);
	//float pdfSpec = pdfGGX(dot(N, H), dot(cross(H, N), getTangent(N)), dot(H, Wo), alpha);
	float pdfSpec = pdfGGXVNDF(N, H, Wo, Wi, roughness * roughness);
	ret.w = mix(pdfDiff, pdfSpec, spec);

	return ret;
}

vec3 trace(Ray ray, int id, SurfaceInfo surfaceInfo, int depth)
{
	vec3 result = vec3(0.0);
	vec3 accumBsdf = vec3(1.0);
	float accumPdf = 1.0;
	vec3 beta = vec3(1.0);

	for (int bounce = 0; ; bounce++)
	{
		if (bounce == depth) break;

		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;

		int matId = texelFetch(matIndices, id).r;
		vec4 samp = getSample(matId, N, Wo);
		vec3 Wi = samp.xyz;

		float pdf = samp.w;
		if (pdf < 1e-12) break;

		vec3 bsdf = bsdf(matId, Wo, Wi, N);

		accumBsdf *= bsdf;
		accumPdf *= pdf;
		beta = accumBsdf / accumPdf;

		Ray newRay;
		newRay.ori = P + Wi * 1e-4;
		newRay.dir = Wi;

		SceneHitInfo scHitInfo = sceneHit(newRay);
		if (scHitInfo.shapeId == -1)
		{
			result += getEnvRadiance(Wi) * beta;
			break;
		}

		vec3 nextP = rayPoint(newRay, scHitInfo.dist);
		surfaceInfo = triangleSurfaceInfo(scHitInfo.shapeId, nextP);
		id = scHitInfo.shapeId;
		newRay.ori = nextP;
		ray = newRay;
	}
	return result;
}

void main()
{
	vec2 texSize = textureSize(lastFrame, 0).xy;
	vec2 texCoord = texSize * scrCoord;
	float noiseX = (noise(texCoord) + 1.0) / 2.0;
	float noiseY = (noise(texCoord.yx) + 1.0) / 2.0;
	texCoord = texSize * vec2(noiseX, noiseY);
	randSeed = (uint(texCoord.x) * freeCounter) + uint(texCoord.y);

	vec3 result = vec3(0.0);
	vec2 texelSize = 1.0 / textureSize(lastFrame, 0).xy;

	vec2 samp = randBox();

	vec2 biasedCoord = scrCoord + texelSize * samp;
	vec2 ndc = biasedCoord * 2.0 - 1.0;
	vec3 rayDir = normalize((camF + (camU * ndc.y + camR * camAsp * ndc.x) * tanFOV));

	Ray ray;
	ray.ori = camPos;
	ray.dir = rayDir;

	SceneHitInfo scHitInfo = sceneHit(ray);
	if (showBVH)
	{
		result = vec3(maxDepth / bvhDepth * 0.2);
	}
	else if (scHitInfo.shapeId == -1)
	{
		result = getEnvRadiance(rayDir);
	}
	else
	{
		ray.ori = rayPoint(ray, scHitInfo.dist);
		SurfaceInfo sInfo = triangleSurfaceInfo(scHitInfo.shapeId, ray.ori);
		result = trace(ray, scHitInfo.shapeId, sInfo, 3);
	}

	vec3 lastRes = texture(lastFrame, scrCoord).rgb;

	if (isnan(result.x) || isnan(result.y) || isnan(result.z)) result = lastRes;
	else result = (lastRes * float(spp) + result) / float(spp + 1);

	if (BUG) result = BUGVAL;

	FragColor = vec4(result, 1.0);
}