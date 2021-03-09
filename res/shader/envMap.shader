=type lib
=include math.shader

uniform float envStrength;
uniform sampler2D envMap;
uniform sampler2D envCdfTable;

const vec3 BRIGHTNESS = vec3(0.299, 0.587, 0.114);

vec3 envGetRadiance(vec3 Wi)
{
	return texture(envMap, sphereToPlane(Wi)).rgb * envStrength;
}

float envFetchCdf(int x, int y)
{
	return texelFetch(envCdfTable, ivec2(x, y), 0).r;
}

float envGetPortion(vec3 Wi)
{
	ivec2 size = textureSize(envMap, 0).xy;
	return dot(envGetRadiance(Wi), BRIGHTNESS) / envFetchCdf(size.x, size.y - 1);
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

	float sum = envFetchCdf(w, h - 1);

	int col, row;
	float rowVal = rand(0.0, sum);

	int l = 0, r = h - 1;
	while (l != r)
	{
		int m = (l + r) >> 1;
		if (rowVal >= envFetchCdf(w, m)) l = m + 1;
		else r = m;
	}
	row = l;

	float colVal = rand(0.0, envFetchCdf(w - 1, row));
	l = 0, r = w - 1;
	while (l != r)
	{
		int m = (l + r) >> 1;
		if (colVal >= envFetchCdf(m, row)) l = m + 1;
		else r = m;
	}
	col = l;

	float sinTheta = sin(Pi * (float(row) + 0.5) / float(h));
	vec2 uv = vec2(float(col) + 0.5, float(row + 0.5)) / vec2(float(w), float(h));
	vec3 Wi = planeToSphere(uv);

	return vec4(Wi, envPdfLi(Wi));
}