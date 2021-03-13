=type vertex
#version 450 core
layout(location = 0) in vec2 pos;

out vec2 scrCoord;

void main()
{
	vec2 ndc = pos * 2.0 - 1.0;
	gl_Position = vec4(ndc, 0.0, 1.0);
	scrCoord = pos;
}

=type fragment
#version 450 core
in vec2 scrCoord;
out vec4 FragColor;

bool BUG = false;
vec3 BUGVAL;

=include math.shader
=include material.shader
=include envMap.shader

struct Ray
{
	vec3 ori;
	vec3 dir;
};

struct SurfaceInfo
{
	vec3 norm;
	vec2 texCoord;
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

struct Light
{
	vec3 va;
	vec3 vb;
	vec3 vc;
	vec3 norm;
	vec3 radiosity;
};

uniform vec3 camF;
uniform vec3 camR;
uniform vec3 camU;
uniform vec3 camPos;
uniform float tanFOV;
uniform float camAsp;
uniform float camNear;
uniform sampler2D lastFrame;
uniform int spp;
uniform int freeCounter;
uniform bool showBVH;
uniform int boxIndex;
uniform float bvhDepth;
uniform bool envImportance;

uniform samplerBuffer vertices;
uniform samplerBuffer normals;
uniform samplerBuffer texCoords;
uniform isamplerBuffer indices;
uniform samplerBuffer bounds;
uniform isamplerBuffer sizeIndices;
uniform isamplerBuffer matIndices;

const int MAX_LIGHTS = 8;
uniform Light lights[MAX_LIGHTS];
uniform int lightCount;

vec3 rayPoint(Ray ray, float t)
{
	return ray.ori + ray.dir * t;
}

vec3 lightRandPoint(int id)
{
	return lights[id].va + (lights[id].vb - lights[id].va) * rand() + (lights[id].vc - lights[id].va) * rand();
}

vec3 lightGetRadiance(int id, vec3 dir, float dist)
{
	return lights[id].radiosity * satDot(lights[id].norm, dir) / (dist * dist);
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

	vec2 ta = texelFetch(texCoords, ia).xy;
	vec2 tb = texelFetch(texCoords, ib).xy;
	vec2 tc = texelFetch(texCoords, ic).xy;

	vec3 pa = a - p;
	vec3 pb = b - p;
	vec3 pc = c - p;

	float areaInv = 1.0 / length(cross(b - a, c - a));
	float la = length(cross(pb, pc)) * areaInv;
	float lb = length(cross(pc, pa)) * areaInv;
	float lc = 1.0 - la - lb;

	ret.norm = normalize(na * la + nb * lb + nc * lc);
	ret.texCoord = ta * la + tb * lb + tc * lc;

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
	if (!quickCheck) dist = 1e8;
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
			if (hInfo.hit && hInfo.dist < dist)
			{
				if (quickCheck) return 0;
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

HitInfo lightHit(int id, Ray ray)
{
	HitInfo ret;

	vec3 vd = lights[id].vc + lights[id].vb - lights[id].va;
	HitInfo l = intersectTriangle(lights[id].va, lights[id].vb, lights[id].vc, ray);
	HitInfo r = intersectTriangle(lights[id].vc, lights[id].vb, vd, ray);

	if (l.hit) return l;
	if (r.hit) return r;

	ret.hit = false;
	return ret;
}

SceneHitInfo sceneHit(Ray ray)
{
	SceneHitInfo ret;
	ret.shapeId = bvhHit(ray, ret.dist, false);

	for (int i = 0; i < lightCount; i++)
	{
		HitInfo h = lightHit(i, ray);
		if (h.hit && h.dist < ret.dist)
		{
			ret.shapeId = -i - 2;
			ret.dist = h.dist;
		}
	}

	return ret;
}

struct EnvSample
{
	vec3 Wi;
	vec3 coef;
	float pdf;
};

EnvSample getEnvSample(vec3 x)
{
	EnvSample samp;
	vec4 sp = envImportanceSample();
	samp.Wi = sp.xyz;
	samp.pdf = sp.w;

	Ray ray;
	ray.ori = x + samp.Wi * 1e-4;
	ray.dir = samp.Wi;
	float dist = 1e6;
	if (bvhHit(ray, dist, true) == 0)
	{
		samp.coef = vec3(0.0);
		return samp;
	}

	samp.coef = envGetRadiance(samp.Wi) / samp.pdf;
	return samp;
}

vec3 trace(Ray ray, int id, SurfaceInfo surfaceInfo, int depth)
{
	vec3 result = vec3(0.0);
	vec3 beta = vec3(1.0);

	for (int bounce = 0; ; bounce++)
	{
		if (bounce == depth) break;

		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;

		int matId = texelFetch(matIndices, id).r;
		vec3 albedo = materials[matId].albedo;
		int type = materials[matId].type;
		
		float arg = (type == MetalWorkflow) ? materials[matId].metallic : materials[matId].ior;
		float roughness = materials[matId].roughness;

		if (envImportance)
		{
			if (type == Dielectric)
			{
				if (!approximateDelta(roughness))
				{
					EnvSample envSamp = getEnvSample(P);
					float bsdfPdf = dielectricPdf(Wo, envSamp.Wi, N, arg, roughness);
					float weight = heuristic(envSamp.pdf, 1, bsdfPdf, 1);
					result += dielectricBsdf(Wo, envSamp.Wi, N, albedo, arg, roughness) * beta * satDot(N, envSamp.Wi) * envSamp.coef * weight;
				}
			}
			else
			{
				EnvSample envSamp = getEnvSample(P);
				float bsdfPdf = metalWorkflowPdf(Wo, envSamp.Wi, N, arg, roughness * roughness);
				float weight = heuristic(envSamp.pdf, 1, bsdfPdf, 1);
				result += metalWorkflowBsdf(Wo, envSamp.Wi, N, albedo, arg, roughness) * beta * satDot(N, envSamp.Wi) * envSamp.coef * weight;
			}
		}

		BsdfSample samp = (type == MetalWorkflow) ?
			metalWorkflowGetSample(N, Wo, albedo, arg, roughness) :
			dielectricGetSample(N, Wo, albedo, arg, roughness);

		vec3 Wi = samp.Wi;
		float bsdfPdf = samp.pdf;
		vec3 bsdf = samp.bsdf;
		uint flag = samp.flag;

		if (bsdfPdf < 1e-8) break;
		beta *= bsdf / bsdfPdf * ((flag == SpecRefl || flag == SpecTrans) ? 1.0 : absDot(N, Wi));

		Ray newRay;
		newRay.ori = P + Wi * 1e-4;
		newRay.dir = Wi;

		SceneHitInfo scHitInfo = sceneHit(newRay);
		if (scHitInfo.shapeId == -1)
		{
			float weight = 1.0;
			if (envImportance)
			{
				if (type != Dielectric || !approximateDelta(roughness))
				{
					float envPdf = envPdfLi(Wi);
					weight = heuristic(bsdfPdf, 1, envPdf, 1);
				}
			}
			result += envGetRadiance(Wi) * beta * weight;
			break;
		}
		
		if (scHitInfo.shapeId < -1)
		{
			//result += lightGetRadiance(-scHitInfo.shapeId - 2, -Wi, scHitInfo.dist) * beta;
			//result += lights[-scHitInfo.shapeId - 2].radiosity * beta * 0.5 * PiInv;
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
		result = envGetRadiance(rayDir);
	}
	else if (scHitInfo.shapeId >= 0)
	{
		ray.ori = rayPoint(ray, scHitInfo.dist);
		SurfaceInfo sInfo = triangleSurfaceInfo(scHitInfo.shapeId, ray.ori);
		result = trace(ray, scHitInfo.shapeId, sInfo, 3);
	}
	else
	{
		result = lights[-scHitInfo.shapeId - 2].radiosity;
	}

	vec3 lastRes = texture(lastFrame, scrCoord).rgb;

	if (isnan(result.x) || isnan(result.y) || isnan(result.z)) result = lastRes;
	else result = (lastRes * float(spp) + result) / float(spp + 1);

	if (BUG) result = BUGVAL;

	FragColor = vec4(result, 1.0);
}