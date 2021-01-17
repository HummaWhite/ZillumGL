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
	return vMin + rand() * (vMax - vMin);
}

vec2 randBox()
{
	return vec2(rand(), rand());
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

float lerp(float a, float b, float x)
{
	return a + x * (b - a);
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
	float lc = length(cross(pa, pb)) * areaInv;

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

mat3 TBNMatrix(vec3 N)
{
	vec3 T = (abs(N.z) > 0.99) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
	vec3 B = normalize(cross(N, T));
	T = normalize(cross(B, N));
	return mat3(T, B, N);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;

	if (NdotH2 < 1e-8) return 0.0;

	float nom = a2;
	float denom = NdotH2 * (a2 - 1.0) + 1.0;
	denom = denom * denom * Pi;
	return nom / max(denom, 1e-7);
}

float geometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = roughness * roughness * 0.5;

	float nom = NdotV;
	float denom = NdotV * (1.0 - k) + k;

	return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);

	float ggx2 = geometrySchlickGGX(NdotV, roughness);
	float ggx1 = geometrySchlickGGX(NdotL, roughness);

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
	vec3 V = sampleUniformHemisphere(N);
	return vec4(V, dot(V, N) * PiInv);
}

float pdfGGX(float NoH, float HoV, float a)
{
	return distributionGGX(NoH, a) * NoH / (4.0 * HoV);
}

vec3 sampleGGXNoPdf(vec2 xi, vec3 N, vec3 Wo, float roughness)
{
	float r4 = pow(roughness, 4.0);
	float phi = 2.0 * Pi * xi.x;
	float sinTheta, cosTheta;

	cosTheta = sqrt((1.0 - xi.y) / (1.0 + (r4 - 1.0) * xi.y));
	sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	H = normalize(TBNMatrix(N) * H);
	return normalize(H * 2.0 * dot(Wo, H) - Wo);
}

vec4 sampleGGX(vec2 xi, vec3 N, vec3 Wo, float roughness)
{
	vec3 Wi = sampleGGXNoPdf(xi, N, Wo, roughness);
	vec3 H = normalize(Wi + Wo);

	float NdotH = dot(N, H);
	float HdotV = dot(H, Wo);

	return vec4(Wi, pdfGGX(NdotH, HdotV, roughness));
}

vec3 sampleGGX(vec3 N, vec3 Wo, float roughness)
{
	return sampleGGXNoPdf(randBox(), N, Wo, roughness);
}

vec3 bsdf(int matId, vec3 Wo, vec3 Wi, vec3 N)
{
	vec3 albedo = materials[matId].albedo;
	float metallic = materials[matId].metallic;
	float roughness = materials[matId].roughness;

	vec3 L = Wi;
	vec3 V = Wo;
	vec3 H = normalize(V + L);

	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3	F = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);
	float	D = distributionGGX(max(dot(N, H), 0.0), roughness);
	float	G = geometrySmith(N, V, L, roughness);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	vec3 FDG = F * D * G;
	float denominator = 4.0 * NdotV * NdotL;
	if (denominator == 0.0) return vec3(0.0);

	vec3 specular = FDG / denominator;

	return (kD * albedo * PiInv + specular) * NdotL;
}

vec4 getSample(int matId, vec3 N, vec3 Wo)
{
	vec4 ret;
	float metallic = materials[matId].metallic;
	float roughness = materials[matId].roughness;

	float pSpec = 1.0 / (2.0 - metallic);
	bool sampleDiffuse = (rand() > pSpec);
	vec3 Wi = sampleDiffuse ? sampleCosineWeighted(N).xyz : sampleGGX(N, Wo, roughness);

	ret.xyz = Wi;
	vec3 H = normalize(Wi + Wo);

	float pdfDiff = dot(Wi, N) * PiInv;
	float pdfSpec = pdfGGX(dot(N, H), dot(H, Wo), roughness);
	ret.w = lerp(pdfDiff, pdfSpec, pSpec);

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
		if (accumPdf < 1e-10) break;
		if (bounce == depth) break;

		int matId = texelFetch(matIndices, id).r;

		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;

		vec4 samp = getSample(matId, N, Wo);
		vec3 Wi = samp.xyz;

		float pdf = samp.w;
		if (abs(pdf) < 1e-6) break;

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
			result += clamp(texture(skybox, sphereToPlane(newRay.dir)).rgb * beta, 0.0, 1e8);
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
		result = texture(skybox, sphereToPlane(rayDir)).rgb;
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

	FragColor = vec4(result, 1.0);
}