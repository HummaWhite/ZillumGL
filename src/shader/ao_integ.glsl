@type lib

@include random.glsl
@include math.glsl
@include intersection.glsl

uniform vec3 uAoCoef;

vec3 aoIntegTrace(Ray ray, int id, SurfaceInfo surfaceInfo, inout Sampler s)
{
	vec3 ao = vec3(0.0);
	vec3 P = ray.ori;
	vec3 N = surfaceInfo.norm;
	if (dot(N, ray.dir) > 0) N = -N;

	for (int i = 0; i < uMaxBounce; i++)
	{
		vec3 Wi = sampleCosineWeighted(N, sample2D(s)).xyz;

		Ray occRay;
		occRay.ori = P + Wi * 1e-4;
		occRay.dir = Wi;

		float tmp = uAoCoef.x;
		if (bvhTest(occRay, tmp)) ao += 1.0;
	}

	return 1.0 - ao / float(uMaxBounce);
}