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
=include intersection.shader
=include envMap.shader

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
uniform bool roulette;
uniform float rouletteProb;
uniform int maxBounce;

vec3 trace(Ray ray, int id, SurfaceInfo surfaceInfo)
{
	vec3 result = vec3(0.0);
	vec3 beta = vec3(1.0);

	for (int bounce = 0; bounce < maxBounce; bounce++)
	{
		vec3 P = ray.ori;
		vec3 Wo = -ray.dir;
		vec3 N = surfaceInfo.norm;

		int matId = texelFetch(matIndices, id).r;
		vec3 albedo = materials[matId].albedo;
		int type = materials[matId].type;
		float metIor = materials[matId].metIor;
		float roughness = materials[matId].roughness;

		if (envImportance)
		{
			if (type == Dielectric)
			{
				if (!approximateDelta(roughness))
				{
					EnvSample envSamp = envImportanceSample(P);
					float bsdfPdf = dielectricPdf(Wo, envSamp.Wi, N, metIor, roughness);
					float weight = heuristic(envSamp.pdf, 1, bsdfPdf, 1);
					result += dielectricBsdf(Wo, envSamp.Wi, N, albedo, metIor, roughness) * beta * satDot(N, envSamp.Wi) * envSamp.coef * weight;
				}
			}
			else
			{
				EnvSample envSamp = envImportanceSample(P);
				float bsdfPdf = metalWorkflowPdf(Wo, envSamp.Wi, N, metIor, roughness * roughness);
				float weight = heuristic(envSamp.pdf, 1, bsdfPdf, 1);
				result += metalWorkflowBsdf(Wo, envSamp.Wi, N, albedo, metIor, roughness) * beta * satDot(N, envSamp.Wi) * envSamp.coef * weight;
			}
		}

		BsdfSample samp = (type == MetalWorkflow) ?
			metalWorkflowGetSample(N, Wo, albedo, metIor, roughness) :
			dielectricGetSample(N, Wo, albedo, metIor, roughness);

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

		if (roulette)
		{
			if (rand() > rouletteProb) break;
			beta /= rouletteProb;
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
		result = trace(ray, scHitInfo.shapeId, sInfo);
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