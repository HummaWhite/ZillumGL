@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;

@include random.glsl
@include math.glsl
@include intersection.glsl
@include camera.glsl

uniform int uSpp;
uniform int uFreeCounter;
uniform int uSampleDim;
uniform sampler2D uNoiseTex;
uniform int uSampleNum;

uniform vec3 uAoCoef;

vec3 aoIntegTrace(Ray ray, int id, SurfaceInfo surf, inout Sampler s)
{
	vec3 ao = vec3(0.0);
	vec3 pos = ray.ori;
	vec3 norm = surf.norm;
	if (dot(norm, ray.dir) > 0) norm = -norm;

	for (int i = 0; i < uMaxBounce; i++)
	{
		vec3 wi = sampleCosineWeighted(norm, sample2D(s)).xyz;
		Ray occRay = rayOffseted(pos, wi);
		if (bvhTest(occRay, uAoCoef.x)) ao += 1.0;
	}
	return 1.0 - ao / float(uMaxBounce);
}