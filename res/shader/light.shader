@type lib
@include envMap.shader

uniform samplerBuffer lightPower;
uniform isamplerBuffer lightAlias;
uniform samplerBuffer lightProb;
uniform int nLights;
uniform float lightSum;
uniform bool lightEnvUniformSample;
uniform float lightSamplePortion;

vec3 lightGetRadiance(int id, vec3 x, vec3 Wo)
{
	int triId = id + objPrimCount;
	SurfaceInfo sInfo = triangleSurfaceInfo(triId, x);
	if (dot(Wo, sInfo.norm) <= 0.0) return vec3(0.0);

	return texelFetch(lightPower, id).rgb / triangleArea(triId) * 0.5f * PiInv;
}

float lightPdfLi(int id, vec3 x, vec3 y)
{
	int triId = id + objPrimCount;
	vec3 N = triangleSurfaceInfo(triId, y).norm;
	vec3 yx = normalize(x - y);
	float cosTheta = absDot(N, yx);
	if (cosTheta < 1e-8) return -1.0;

	return distSquare(x, y) / (triangleArea(triId) * cosTheta);
}

LightSample lightSampleOne(vec3 x, vec4 u)
{
	LightSample ret;

	int cx = int(float(nLights) * u.x);
	float cy = u.y;

	int id = (cy < texelFetch(lightProb, cx).r) ? cx : texelFetch(lightAlias, cx).r;
	int triId = id + objPrimCount;

	int ia = texelFetch(indices, triId * 3 + 0).r;
	int ib = texelFetch(indices, triId * 3 + 1).r;
	int ic = texelFetch(indices, triId * 3 + 2).r;

	vec3 a = texelFetch(vertices, ia).xyz;
	vec3 b = texelFetch(vertices, ib).xyz;
	vec3 c = texelFetch(vertices, ic).xyz;

	vec3 y = sampleTriangleUniform(a, b, c, u.zw);
	vec3 Wi = normalize(y - x);
	vec3 N = triangleSurfaceInfo(triId, y).norm;
	float cosTheta = dot(N, -Wi);
	if (cosTheta < 1e-6) return INVALID_LIGHT_SAMPLE;

	Ray lightRay;
	lightRay.ori = x + Wi * 1e-4;
	lightRay.dir = Wi;

	float dist = distance(x, y);
	float pdf = dist * dist / (triangleArea(a, b, c) * cosTheta);

	float testDist = dist - 1e-4 - 1e-6;
	if (bvhTest(lightRay, testDist) || pdf < 1e-8) return INVALID_LIGHT_SAMPLE;

	vec3 weight = lightGetRadiance(id, y, -Wi);

	float pdfSample = dot(texelFetch(lightPower, id).rgb, BRIGHTNESS) / lightSum;
	pdf *= pdfSample;
	return lightSample(Wi, weight / pdf, pdf);
}

LightSample sampleLightAndEnv(vec3 x, float ud, vec4 us)
{
	float pdfSampleLight = 0.0;

	if (nLights > 0)
		pdfSampleLight = lightEnvUniformSample ? lightSamplePortion : lightSum / (lightSum + envSum);

	bool sampleLight = ud < pdfSampleLight;
	float pdfSelect = sampleLight ? pdfSampleLight : 1.0 - pdfSampleLight;

	LightSample samp = sampleLight ? lightSampleOne(x, us) : envImportanceSample(x, us);
	samp.coef /= pdfSelect;
	samp.pdf *= pdfSelect;
	return samp;
}

float pdfSelectLight(int id)
{
	float fstPdf = dot(texelFetch(lightPower, id).rgb, BRIGHTNESS) / lightSum;
	float sndPdf = lightEnvUniformSample ? lightSamplePortion : lightSum / (lightSum + envSum);
	return fstPdf * sndPdf;
}

float pdfSelectEnv()
{
	return lightEnvUniformSample ? (1.0 - lightSamplePortion) : envSum / (lightSum + envSum);
}