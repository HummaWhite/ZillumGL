@type lib
@include math.glsl

uniform sampler2D uEnvMap;
uniform isampler2D uEnvAliasTable;
uniform sampler2D uEnvAliasProb;
uniform float uEnvSum;
uniform float uEnvRotation;

struct LightSample
{
	vec3 Wi;
	vec3 coef;
	float pdf;
};

LightSample lightSample(vec3 Wi, vec3 coef, float pdf)
{
	LightSample ret;
	ret.Wi = Wi;
	ret.coef = coef;
	ret.pdf = pdf;
	return ret;
}

const LightSample INVALID_LIGHT_SAMPLE = lightSample(vec3(0.0), vec3(0.0), 0.0);

vec3 envGetRadiance(vec3 Wi)
{
	Wi = rotateZ(Wi, -uEnvRotation);
	return texture(uEnvMap, sphereToPlane(Wi)).rgb;
}

float envGetPortion(vec3 Wi)
{
	return luminance(envGetRadiance(Wi)) / uEnvSum;
}

float envPdfLi(vec3 Wi)
{
	if (uEnvSum == 0.0) return 0.0;
	vec2 size = vec2(textureSize(uEnvMap, 0).xy);
	return envGetPortion(Wi) * size.x * size.y * 0.5f * square(PiInv);// / sqrt(1.0 - square(Wi.z));
}

vec4 envImportanceSample(vec4 u)
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
	vec3 Wi = planeToSphere(uv);
	//float pdf = envPdfLi(Wi);
	Wi = rotateZ(Wi, uEnvRotation);
	float pdf = envGetPortion(Wi) * size.x* size.y * 0.5f * square(PiInv);
	return vec4(Wi, pdf);
}

LightSample envImportanceSample(vec3 x, vec4 u)
{
	vec4 sp = envImportanceSample(u);
	vec3 Wi = sp.xyz;
	float pdf = sp.w;

	Ray ray;
	ray.ori = x + Wi * 1e-4;
	ray.dir = Wi;
	float dist = 1e8;
	if (bvhTest(ray, dist) || pdf == 0.0)
		return INVALID_LIGHT_SAMPLE;

	return lightSample(Wi, envGetRadiance(Wi) / pdf, pdf);
}