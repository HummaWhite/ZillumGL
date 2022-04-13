@type lib

uniform samplerBuffer uLightPower;
uniform isamplerBuffer uLightAlias;
uniform samplerBuffer uLightProb;
uniform int uNumLightTriangles;
uniform float uLightSum;
uniform bool uLightEnvUniformSample;
uniform float uLightSamplePortion;
uniform int uObjPrimCount;

uniform sampler2D uEnvMap;
uniform isampler2D uEnvAliasTable;
uniform sampler2D uEnvAliasProb;
uniform float uEnvSum;
uniform float uEnvRotation;

struct LightPdf
{
	float pdfPos;
	float pdfDir;
};

LightPdf makeLightPdf(float pdfPos, float pdfDir)
{
	LightPdf ret;
	ret.pdfPos = pdfPos;
	ret.pdfDir = pdfDir;
	return ret;
}

struct LightLiSample
{
	vec3 wi;
	vec3 coef;
	float pdf;
};

LightLiSample makeLightLiSample(vec3 wi, vec3 coef, float pdf)
{
	LightLiSample ret;
	ret.wi = wi;
	ret.coef = coef;
	ret.pdf = pdf;
	return ret;
}

const LightLiSample InvalidLiSample = makeLightLiSample(vec3(0.0), vec3(0.0), 0.0);

struct LightLeSample
{
	Ray ray;
	vec3 Le;
	float pdfPos;
	float pdfDir;
};

LightLeSample makeLightLeSample(Ray ray, vec3 Le, float pdfPos, float pdfDir)
{
	LightLeSample ret;
	ret.ray = ray;
	ret.Le = Le;
	ret.pdfPos = pdfPos;
	ret.pdfDir = pdfDir;
	return ret;
}

int lightSampleOne(vec2 u)
{
	int cx = int(float(uNumLightTriangles) * u.x);
	return (u.y < texelFetch(uLightProb, cx).r) ? cx : texelFetch(uLightAlias, cx).r;
}

float lightPdfSampleOne(int id)
{
	return luminance(texelFetch(uLightPower, id).rgb) / uLightSum;
}

vec3 lightLe(int id, vec3 x, vec3 wo)
{
	int triId = id + uObjPrimCount;
	SurfaceInfo sInfo = triangleSurfaceInfo(triId, x);
	if (dot(wo, sInfo.ng) <= 0.0)
		return vec3(0.0);
	return texelFetch(uLightPower, id).rgb / triangleArea(triId) * 0.5f * PiInv;
}

float lightPdfLi(int id, vec3 x, vec3 y)
{
	int triId = id + uObjPrimCount;
	vec3 norm = triangleSurfaceInfo(triId, y).ng;
	vec3 yx = normalize(x - y);
	float cosTheta = absDot(norm, yx);
	if (cosTheta < 1e-8)
		return -1.0;

	return distSquare(x, y) / (triangleArea(triId) * cosTheta);
}

LightPdf lightPdfLe(int id, Ray ray)
{
	LightPdf ret;
	int triId = id + uObjPrimCount;
	vec3 norm = triangleSurfaceInfo(triId, ray.ori).ng;

	ret.pdfPos = 1.0 / triangleArea(triId);
	ret.pdfDir = (dot(norm, ray.dir) <= 0) ? 0 : 0.5 * PiInv;
	return ret;
}

LightLeSample lightSampleOneLe(int id, vec4 u)
{
	int triId = id + uObjPrimCount;
	vec3 ori = triangleSampleUniform(triId, u.xy);
	vec3 norm = triangleSurfaceInfo(triId, ori).ng;
	vec4 samp = sampleCosineWeighted(norm, u.zw);

	return makeLightLeSample(rayOffseted(makeRay(ori, samp.xyz)), lightLe(id, ori, samp.xyz),
		1.0 / triangleArea(triId), samp.w);
}

LightLiSample lightSampleLi(int id, vec3 x, vec2 u)
{
	int triId = id + uObjPrimCount;

	int ia = texelFetch(uIndices, triId * 3 + 0).r;
	int ib = texelFetch(uIndices, triId * 3 + 1).r;
	int ic = texelFetch(uIndices, triId * 3 + 2).r;

	vec3 a = texelFetch(uVertices, ia).xyz;
	vec3 b = texelFetch(uVertices, ib).xyz;
	vec3 c = texelFetch(uVertices, ic).xyz;

	vec3 y = sampleTriangleUniform(a, b, c, u);
	vec3 wi = normalize(y - x);
	vec3 norm = triangleSurfaceInfo(triId, y).ng;
	float cosTheta = dot(norm, -wi);
	if (cosTheta < 1e-6)
		return InvalidLiSample;

	Ray lightRay = rayOffseted(x, wi);

	float dist = distance(x, y);
	float pdf = dist * dist / (triangleArea(a, b, c) * cosTheta);

	float testDist = dist - 1e-4 - 1e-6;
	if (bvhTest(lightRay, testDist) || pdf < 1e-8)
		return InvalidLiSample;

	vec3 weight = lightLe(id, y, -wi);

	float pdfSample = luminance(texelFetch(uLightPower, id).rgb) / uLightSum;
	pdf *= pdfSample;
	return makeLightLiSample(wi, weight / pdf, pdf);
}

LightLiSample lightSampleOneLi(vec3 x, vec4 u)
{
	int id = lightSampleOne(u.xy);
	return lightSampleLi(id, x, u.zw);
}

vec3 envLe(vec3 wi)
{
	wi = rotateZ(wi, -uEnvRotation);
	return texture(uEnvMap, sphereToPlane(wi)).rgb;
}

float envGetPortion(vec3 wi)
{
	return luminance(envLe(wi)) / uEnvSum;
}

float envPdfLi(vec3 wi)
{
	if (uEnvSum == 0.0) return 0.0;
	vec2 size = vec2(textureSize(uEnvMap, 0).xy);
	return envGetPortion(wi) * size.x * size.y * 0.5f * square(PiInv);// / sqrt(1.0 - square(wi.z));
}

vec4 envSampleWi(vec4 u)
{
	ivec2 size = textureSize(uEnvMap, 0).xy;
	int w = size.x, h = size.y;

	int rx = int(float(h) * u.x);
	float ry = u.y;

	ivec2 rTex = ivec2(w, rx);
	int row = (ry < texelFetch(uEnvAliasProb, rTex, 0).r) ? rx : texelFetch(uEnvAliasTable, rTex, 0).r;

	int cx = int(float(w) * u.z);
	float cy = u.w;

	ivec2 cTex = ivec2(cx, row);
	int col = (cy < texelFetch(uEnvAliasProb, cTex, 0).r) ? cx : texelFetch(uEnvAliasTable, cTex, 0).r;

	float sinTheta = sin(Pi * (float(row) + 0.5) / float(h));
	vec2 uv = vec2(float(col) + 0.5, float(row + 0.5)) / vec2(float(w), float(h));
	vec3 wi = planeToSphere(uv);
	//float pdf = envPdfLi(wi);
	wi = rotateZ(wi, uEnvRotation);
	float pdf = envGetPortion(wi) * size.x * size.y * 0.5f * square(PiInv);
	return vec4(wi, pdf);
}

LightLiSample envSampleLi(vec3 x, vec4 u)
{
	vec4 sp = envSampleWi(u);
	vec3 wi = sp.xyz;
	float pdf = sp.w;

	Ray ray = rayOffseted(x, wi);
	float dist = 1e8;
	if (bvhTest(ray, dist) || pdf == 0.0)
		return InvalidLiSample;

	return makeLightLiSample(wi, envLe(wi) / pdf, pdf);
}

LightLiSample sampleLightAndEnv(vec3 x, float ud, vec4 us)
{
	float pdfSampleLight = 0.0;

	if (uNumLightTriangles > 0)
		pdfSampleLight = uLightEnvUniformSample ? uLightSamplePortion : uLightSum / (uLightSum + uEnvSum);

	bool sampleLight = ud < pdfSampleLight;
	float pdfSelect = sampleLight ? pdfSampleLight : 1.0 - pdfSampleLight;

	LightLiSample samp = sampleLight ? lightSampleOneLi(x, us) : envSampleLi(x, us);
	samp.coef /= pdfSelect;
	samp.pdf *= pdfSelect;
	return samp;
}

float pdfSelectLight(int id)
{
	float fstPdf = luminance(texelFetch(uLightPower, id).rgb) / uLightSum;
	float sndPdf = uLightEnvUniformSample ? uLightSamplePortion : uLightSum / (uLightSum + uEnvSum);
	return fstPdf * sndPdf;
}

float pdfSelectEnv()
{
	return uLightEnvUniformSample ? (1.0 - uLightSamplePortion) : uEnvSum / (uLightSum + uEnvSum);
}