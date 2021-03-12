=type lib
=include math.shader

uniform float envStrength;
uniform sampler2D envMap;
uniform isampler2D envAliasTable;
uniform sampler2D envAliasProb;
uniform float envSum;

const vec3 BRIGHTNESS = vec3(0.299, 0.587, 0.114);

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
	vec2 size = vec2(textureSize(envMap, 0).xy);
	return envGetPortion(Wi) * size.x * size.y * 0.5f * square(PiInv);
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