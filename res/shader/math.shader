=type lib
=include random.shader

const float Pi = 3.14159265358979323846;
const float PiInv = 1.0 / Pi;
const float INF = 1.0 / 0.0;

float square(float x)
{
	return x * x;
}

float heuristic(float pf, int fn, float pg, int gn)
{
	float f = pf * float(fn);
	float g = pg * float(gn);
	return f * f / (f * f + g * g);
}

vec2 toConcentricDisk(vec2 v)
{
	if (v.x == 0.0 && v.y == 0.0) return vec2(0.0, 0.0);
	v = v * 2.0 - 1.0;
	float phi, r;
	if (v.x * v.x > v.y * v.y)
	{
		r = v.x;
		phi = Pi * v.y / v.x * 0.25;
	}
	else
	{
		r = v.y;
		phi = Pi * 0.5 - Pi * v.x / v.y * 0.25;
	}
	return vec2(r * cos(phi), r * sin(phi));
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

float satDot(vec3 a, vec3 b)
{
	return max(dot(a, b), 0.0);
}

float absDot(vec3 a, vec3 b)
{
	return abs(dot(a, b));
}

vec2 sphereToPlane(vec3 uv)
{
	float theta = atan(uv.y, uv.x);
	if (theta < 0.0) theta += Pi * 2.0;
	float phi = atan(length(uv.xy), uv.z);
	return vec2(theta * PiInv * 0.5, phi * PiInv);
}

vec3 planeToSphere(vec2 uv)
{
	float theta = uv.x * Pi * 2.0;
	float phi = uv.y * Pi;
	return vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
}

vec3 getTangent(vec3 N)
{
	return (abs(N.z) > 0.999) ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
}

mat3 tbnMatrix(vec3 N)
{
	vec3 T = getTangent(N);
	vec3 B = normalize(cross(N, T));
	T = cross(B, N);
	return mat3(T, B, N);
}

vec3 normalToWorld(vec3 N, vec3 v)
{
	return normalize(tbnMatrix(N) * v);
}

vec3 sampleUniformHemisphere(vec3 N)
{
	float theta = rand(0.0, Pi * 2.0);
	float phi = rand(0.0, Pi * 0.5);
	vec3 v = vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
	return normalToWorld(N, v);
}

vec4 sampleCosineWeighted(vec3 N)
{
	vec3 vec = sampleUniformHemisphere(N);
	return vec4(vec, satDot(vec, N) * PiInv);
}