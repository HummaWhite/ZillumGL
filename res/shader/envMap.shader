=type lib
=include math.shader

uniform float envStrength;
uniform sampler2D envMap;
uniform isampler2D envAliasTable;
uniform sampler2D envAliasProb;
uniform float envSum;
uniform bool sampleLight;

const vec3 BRIGHTNESS = vec3(0.299, 0.587, 0.114);

struct LightSample
{
	vec3 Wi;
	vec3 coef;
	float pdf;
};

vec3 envGetRadiance(vec3 Wi)
{
	return texture(envMap, sphereToPlane(Wi)).rgb * envStrength;
}

float envGetPortion(vec3 Wi)
{
	return dot(envGetRadiance(Wi), BRIGHTNESS) / envSum;
}

float envPdfLi(vec3 Wi)
{
	if (envSum == 0.0) return 0.0;
	vec2 size = vec2(textureSize(envMap, 0).xy);
	return envGetPortion(Wi) * size.x * size.y * 0.5f * square(PiInv);// / sqrt(1.0 - square(Wi.z));
}

vec4 envImportanceSample()
{
	ivec2 size = textureSize(envMap, 0).xy;
	int w = size.x, h = size.y;

	int rx = int(randUint(0, h));
	float ry = rand();

	ivec2 rTex = ivec2(w, rx);
	int row = (ry < texelFetch(envAliasProb, rTex, 0).r) ? rx : texelFetch(envAliasTable, rTex, 0).r;

	int cx = int(randUint(0, w));
	float cy = rand();

	ivec2 cTex = ivec2(cx, row);
	int col = (cy < texelFetch(envAliasProb, cTex, 0).r) ? cx : texelFetch(envAliasTable, cTex, 0).r;

	float sinTheta = sin(Pi * (float(row) + 0.5) / float(h));
	vec2 uv = vec2(float(col) + 0.5, float(row + 0.5)) / vec2(float(w), float(h));
	vec3 Wi = planeToSphere(uv);
	float pdf = envPdfLi(Wi);

	return vec4(Wi, pdf);
}

LightSample envImportanceSample(vec3 x)
{
	LightSample samp;
	vec4 sp = envImportanceSample();
	samp.Wi = sp.xyz;
	samp.pdf = sp.w;

	Ray ray;
	ray.ori = x + samp.Wi * 1e-4;
	ray.dir = samp.Wi;
	float dist = 1e8;
	if (bvhTest(ray, dist))
	{
		samp.coef = vec3(0.0);
		return samp;
	}

	if (samp.pdf != 0.0)
	{
		samp.coef = envGetRadiance(samp.Wi) / samp.pdf;
	}
	return samp;
}