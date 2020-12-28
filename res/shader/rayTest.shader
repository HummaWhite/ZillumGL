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

struct Ray
{
	vec3 ori;
	vec3 dir;
};

struct Sphere
{
	vec3 center;
	float radius;
	vec3 albedo;
	float metallic;
	float roughness;
};

struct Square
{
	vec3 va;
	vec3 vb;
	vec3 vc;
};

struct SurfaceInfo
{
	vec3 norm;
	int id;
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

uniform vec3 camF;
uniform vec3 camR;
uniform vec3 camU;
uniform vec3 camPos;
uniform float tanFOV;
uniform float camAsp;
uniform float camNear;
uniform vec3 background;
uniform int sphereCount;
uniform Sphere sphereList[4];
uniform sampler2D skybox;
uniform sampler2D lastFrame;
uniform int spp;
uniform int maxSpp;

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

vec2 sphereToPlane(vec3 uv)
{
	float theta = atan(uv.y, uv.x);
	if (theta < 0.0) theta += Pi * 2.0;
	float phi = atan(length(uv.xy), uv.z);
	return vec2(theta / Pi * 0.5, phi / Pi);
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

HitInfo intersectSphere(int id, Ray ray)
{
	vec3 d = ray.dir;
	vec3 o = ray.ori;
	vec3 c = sphereList[id].center;

	float t = dot(d, c - o) / dot(d, d);
	float r = sphereList[id].radius;

	float e = length(o + d * t - c);

	HitInfo ret;
	ret.dist = 0.0;
	if (e > r)
	{
		ret.hit = false;
		return ret;
	}

	float q = sqrt(max(r * r - e * e, 0.0));
	if (length(o - c) < r)
	{
		ret.dist = t + q;
		ret.hit = (ret.dist >= 0.0);
		return ret;
	}

	ret.dist = t - q;
	ret.hit = (ret.dist >= 0.0);
	return ret;
}

SurfaceInfo sphereSurfaceInfo(int id, vec3 p)
{
	SurfaceInfo ret;
	ret.norm = normalize(p - sphereList[id].center);
	ret.id = id;
	return ret;
}

SceneHitInfo sceneHit(Ray ray)
{
	SceneHitInfo ret;
	ret.shapeId = -1;
	ret.dist = 1e4;

	for (int i = 0; i < sphereCount; i++)
	{
		HitInfo hInfo = intersectSphere(i, ray);

		if (hInfo.hit && hInfo.dist < ret.dist)
		{
			ret.shapeId = i;
			ret.dist = hInfo.dist;
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

float distributionGGX(vec3 N, vec3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = NdotH2 * (a2 - 1.0) + 1.0 + 1e-6;
	denom = denom * denom * Pi;

	return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = roughness * roughness / 2.0;

	float nom = NdotV;
	float denom = NdotV * (1.0f - k) + k;

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

vec4 sampleCosineWeighted(vec3 N)
{
	float theta = rand(0.0, Pi * 2.0);
	float phi = rand(0.0, Pi * 0.5);
	vec3 randomVec = vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
	return vec4(normalize(TBNMatrix(N) * randomVec), cos(phi) / Pi);
}

vec4 sampleGGX(vec2 xi, vec3 N, vec3 Wo, float roughness)
{
	float r4 = pow(roughness, 4.0f);
	float phi = 2.0 * Pi * xi.x;
	float sinTheta, cosTheta;

	if (xi.y > 0.99999)
	{
		cosTheta = 0.0f;
		sinTheta = 1.0f;
	}
	else
	{
		cosTheta = sqrt((1.0f - xi.y) / (1.0f + (r4 - 1.0f) * xi.y));
		sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	}

	vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	H = normalize(TBNMatrix(N) * H);
	vec3 L = normalize(H * 2.0f * dot(Wo, H) - Wo);
	vec3 V = Wo;

	float NdotH = max(dot(N, H), 0.0f);
	float HdotV = max(dot(H, V), 0.0f);

	float D = distributionGGX(N, H, roughness);
	float pdf = D * NdotH / (4.0 * HdotV + 1e-8f);

	if (HdotV < 1e-6f) pdf = 0.0f;
	if (pdf < 1e-10f) pdf = 0.0f;
	return vec4(L, pdf);
}

vec4 sampleGGX(vec3 N, vec3 Wo, float roughness)
{
	return sampleGGX(randBox(), N, Wo, roughness);
}

vec3 bsdf(int id, vec3 Wo, vec3 Wi, vec3 N)
{
	vec3 albedo		= sphereList[id].albedo;
	float metallic	= sphereList[id].metallic;
	float roughness = sphereList[id].roughness;

	vec3 L = Wi;
	vec3 V = Wo;
	vec3 H = normalize(V + L);

	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3	F = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);
	float	D = distributionGGX(N, H, roughness);
	float	G = geometrySmith(N, V, L, roughness);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	vec3 FDG = F * D * G;
	float denominator = 4.0 * NdotV * NdotL + 1e-12;
	vec3 specular = FDG / denominator;

	return (kD * albedo / Pi + specular) * NdotL;
}

vec3 bsdf(int id, vec3 Wo, vec3 Wi, vec3 N, bool diffuse)
{
	vec3 albedo = sphereList[id].albedo;
	float metallic = sphereList[id].metallic;
	float roughness = sphereList[id].roughness;

	vec3 L = Wi;
	vec3 V = Wo;
	vec3 H = normalize(V + L);

	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);

	vec3 F0 = mix(vec3(0.04), albedo, metallic);

	vec3 F = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);

	if (diffuse)
	{
		vec3 kD = (1.0 - F) * (1.0 - metallic);
		return kD * albedo / Pi * NdotL;
	}

	float D = distributionGGX(N, H, roughness);
	float G = geometrySmith(N, V, L, roughness);

	vec3 FDG = F * D * G;
	float denominator = 4.0 * NdotV * NdotL + 1e-12;
	vec3 specular = FDG / denominator;

	return specular * NdotL;
}

vec4 getSample(int id, vec3 N, vec3 Wo)
{
	bool sampleDiffuse = (rand() < 0.5 * (1.0 - sphereList[id].metallic));
	vec4 sp = sampleDiffuse ? sampleCosineWeighted(N) : sampleGGX(N, Wo, sphereList[id].roughness);
	if (sampleDiffuse) sp.w = -sp.w;
	return sp;
}

vec3 trace(Ray ray, SurfaceInfo surfaceInfo, int depth)
{
	vec3 result = vec3(0.0);
	vec3 accumBsdf = vec3(1.0);
	float accumPdf = 1.0;
	vec3 beta = vec3(1.0);

	for (int bounce = 1; ; bounce++)
	{
		if (accumPdf < 1e-8) break;

		if (bounce == depth) break;

		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;
		int id = surfaceInfo.id;

		vec4 samp = getSample(id, N, Wo);
		vec3 Wi = samp.xyz;
		float pdf = samp.w;
		if (abs(pdf) < 1e-8) break;

		vec3 bsdf = bsdf(id, Wo, Wi, N, pdf < 0.0);
		if (pdf < 0.0) pdf = -pdf;

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
		surfaceInfo = sphereSurfaceInfo(scHitInfo.shapeId, nextP);
		newRay.ori = nextP;
		ray = newRay;
	}
	return result;
}

void main()
{
	vec2 texSize = textureSize(lastFrame, 0).xy;
	vec2 texCoord = texSize * scrCoord;
	randSeed = uint(texCoord.x) * spp + uint(texCoord.y);

	vec3 result = vec3(0.0f);
	vec2 texelSize = 1.0 / textureSize(lastFrame, 0).xy;

	vec2 samp = randBox();

	vec2 biasedCoord = scrCoord + texelSize * samp;
	vec2 ndc = biasedCoord * 2.0 - 1.0;
	vec3 rayDir = normalize((camF + (camU * ndc.y + camR * camAsp * ndc.x) * tanFOV));

	Ray ray;
	ray.ori = camPos;
	ray.dir = rayDir;

	SceneHitInfo scHitInfo = sceneHit(ray);
	if (scHitInfo.shapeId == -1)
	{
		result = texture(skybox, sphereToPlane(rayDir)).rgb;
	}
	else
	{
		ray.ori = rayPoint(ray, scHitInfo.dist);
		SurfaceInfo sInfo = sphereSurfaceInfo(scHitInfo.shapeId, ray.ori);
		result = trace(ray, sInfo, 5);
	}

	vec3 lastRes = texture(lastFrame, scrCoord).rgb;
	result = (lastRes * float(spp) + result) / float(spp + 1);

	FragColor = vec4(result, 1.0);
}