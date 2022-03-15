@type lib
@include random.glsl

const float Pi = 3.14159265358979323846;
const float PiInv = 1.0 / Pi;
const float INF = 1.0 / 0.0;

float square(float x)
{
	return x * x;
}

float powerHeuristic(float pf, int fn, float pg, int gn)
{
	float f = pf * float(fn);
	float g = pg * float(gn);
	return f * f / (f * f + g * g);
}

float biHeuristic(float f, float g)
{
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

float satDot(vec3 a, vec3 b)
{
	return max(dot(a, b), 0.0);
}

float absDot(vec3 a, vec3 b)
{
	return abs(dot(a, b));
}

float distSquare(vec3 x, vec3 y)
{
	return dot(x - y, x - y);
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

vec3 sampleUniformHemisphere(vec3 N, vec2 u)
{
	float theta = u.x * 2.0 * Pi;
	float phi = u.y * 0.5 * Pi;
	vec3 v = vec3(cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi));
	return normalToWorld(N, v);
}

vec4 sampleCosineWeighted(vec3 N, vec2 u)
{
	vec2 uv = toConcentricDisk(u);
	float z = sqrt(1.0 - dot(uv, uv));
	vec3 v = normalToWorld(N, vec3(uv, z));
	return vec4(v, PiInv * z);
}

bool sameHemisphere(vec3 N, vec3 A, vec3 B)
{
	return dot(N, A) * dot(N, B) > 0;
}

int maxExtent(vec3 v)
{
	if (v.x > v.y)
		return v.x > v.z ? 0 : 2;
	else
		return v.y > v.z ? 1 : 2;
}

float maxComponent(vec3 v)
{
	return max(v.x, max(v.y, v.z));
}

int cubemapFace(vec3 dir)
{
	int maxDim = maxExtent(abs(dir));
	if (maxDim == 0) return dir.x > 0 ? 0 : 1;
	if (maxDim == 1) return dir.y > 0 ? 2 : 3;
	if (maxDim == 2) return dir.z > 0 ? 4 : 5;
}

float angleBetween(vec3 x, vec3 y)
{
	if (dot(x, y) < 0.0) return Pi - 2.0 * asin(length(x + y) * 0.5);
	else return 2.0 * asin(length(y - x) * 0.5);
}

vec3 sampleTriangleUniform(vec3 va, vec3 vb, vec3 vc, vec2 uv)
{
	float r = sqrt(uv.y);
	float u = 1.0 - r;
	float v = uv.x * r;
	return va * (1.0 - u - v) + vb * u + vc * v;
}

float triangleArea(vec3 va, vec3 vb, vec3 vc)
{
	return 0.5f * length(cross(vc - va, vb - va));
}

float triangleSphericalArea(vec3 a, vec3 b, vec3 c)
{
	vec3 ab = cross(a, b);
	vec3 bc = cross(b, c);
	vec3 ca = cross(c, a);

	if (dot(ab, ab) == 0.0 || dot(bc, bc) == 0.0 || dot(ca, ca) == 0.0) return 0.0;
	ab = normalize(ab);
	bc = normalize(bc);
	ca = normalize(ca);

	float alpha = angleBetween(ab, -ca);
	float beta = angleBetween(bc, -ab);
	float gamma = angleBetween(ca, -bc);

	return abs(alpha + beta + gamma - Pi);
}

float triangleSolidAngle(vec3 a, vec3 b, vec3 c)
{
	return triangleSphericalArea(a, b, c);
}

float triangleSolidAngle(vec3 va, vec3 vb, vec3 vc, vec3 p)
{
	return triangleSolidAngle(normalize(va - p), normalize(vb - p), normalize(vc - p));
}

vec3 rotateZ(vec3 v, float angle)
{
	float cost = cos(angle);
	float sint = sin(angle);
	return vec3(vec2(v.x * cost - v.y * sint, v.x * sint + v.y * cost), v.z);
}

float pow5(float x)
{
	float x2 = x * x;
	return x2 * x2 * x;
}

float luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

bool isBlack(vec3 color)
{
	return luminance(color) < 1e-5f;
}

bool hasNan(vec3 color)
{
	return isnan(color.x) || isnan(color.y) || isnan(color.z);
}