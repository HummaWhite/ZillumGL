@type lib
@include envMap.shader

uniform samplerBuffer uLightPower;
uniform isamplerBuffer uLightAlias;
uniform samplerBuffer uLightProb;
uniform int uNumLightTriangles;
uniform float uLightSum;
uniform bool uLightEnvUniformSample;
uniform float uLightSamplePortion;
uniform int uObjPrimCount;

vec3 lightGetRadiance(int id, vec3 x, vec3 Wo)
{
	int triId = id + uObjPrimCount;
	SurfaceInfo sInfo = triangleSurfaceInfo(triId, x);
	if (dot(Wo, sInfo.norm) <= 0.0) return vec3(0.0);

	return texelFetch(uLightPower, id).rgb / triangleArea(triId) * 0.5f * PiInv;
}

float lightPdfLi(int id, vec3 x, vec3 y)
{
	int triId = id + uObjPrimCount;
	vec3 N = triangleSurfaceInfo(triId, y).norm;
	vec3 yx = normalize(x - y);
	float cosTheta = absDot(N, yx);
	if (cosTheta < 1e-8) return -1.0;

	return distSquare(x, y) / (triangleArea(triId) * cosTheta);
}

LightSample lightSampleOne(vec3 x, vec4 u)
{
	LightSample ret;

	int cx = int(float(uNumLightTriangles) * u.x);
	float cy = u.y;

	int id = (cy < texelFetch(uLightProb, cx).r) ? cx : texelFetch(uLightAlias, cx).r;
	int triId = id + uObjPrimCount;

	int ia = texelFetch(uIndices, triId * 3 + 0).r;
	int ib = texelFetch(uIndices, triId * 3 + 1).r;
	int ic = texelFetch(uIndices, triId * 3 + 2).r;

	vec3 a = texelFetch(uVertices, ia).xyz;
	vec3 b = texelFetch(uVertices, ib).xyz;
	vec3 c = texelFetch(uVertices, ic).xyz;

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

	float pdfSample = dot(texelFetch(uLightPower, id).rgb, BRIGHTNESS) / uLightSum;
	pdf *= pdfSample;
	return lightSample(Wi, weight / pdf, pdf);
}

LightSample sampleLightAndEnv(vec3 x, float ud, vec4 us)
{
	float pdfSampleLight = 0.0;

	if (uNumLightTriangles > 0)
		pdfSampleLight = uLightEnvUniformSample ? uLightSamplePortion : uLightSum / (uLightSum + uEnvSum);

	bool sampleLight = ud < pdfSampleLight;
	float pdfSelect = sampleLight ? pdfSampleLight : 1.0 - pdfSampleLight;

	LightSample samp = sampleLight ? lightSampleOne(x, us) : envImportanceSample(x, us);
	samp.coef /= pdfSelect;
	samp.pdf *= pdfSelect;
	return samp;
}

float pdfSelectLight(int id)
{
	float fstPdf = dot(texelFetch(uLightPower, id).rgb, BRIGHTNESS) / uLightSum;
	float sndPdf = uLightEnvUniformSample ? uLightSamplePortion : uLightSum / (uLightSum + uEnvSum);
	return fstPdf * sndPdf;
}

float pdfSelectEnv()
{
	return uLightEnvUniformSample ? (1.0 - uLightSamplePortion) : uEnvSum / (uLightSum + uEnvSum);
}