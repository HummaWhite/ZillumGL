@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;

bool BUG = false;
vec3 BUGVAL;

@include random.glsl
@include math.glsl
@include material.glsl
@include intersection.glsl
@include env_light.glsl
@include light.glsl
@include camera.glsl

uniform bool uShowBVH;
uniform int uMatIndex;
uniform float uBvhDepth;
uniform int uMaxBounce;

uniform samplerBuffer uMaterials;
uniform isamplerBuffer uMatTypes;
uniform isamplerBuffer uMatTexIndices;

uniform int uNumTextures;
uniform sampler2DArray uTextures;
uniform samplerBuffer uTexUVScale;

uniform int uSpp;
uniform int uFreeCounter;
uniform int uSampleDim;
uniform sampler2D uNoiseTex;
uniform int uSampleNum;
uniform bool uAoMode;

@include path_integ.glsl
@include ao_integ.glsl

vec3 getResult(Ray ray, inout Sampler s)
{
	vec3 result = vec3(0.0);

	if (uShowBVH)
	{
		float maxDepth;
		int primId = bvhDebug(ray, maxDepth);
		int matId = texelFetch(uMatTexIndices, primId).r & 0xffff;
		if (matId == uMatIndex && primId != -1) result = vec3(1.0, 1.0, 0.2);
		else result = vec3(maxDepth / uBvhDepth * 0.2);
	}
	else
	{
		float dist;
		int primId = bvhHit(ray, dist);
		vec3 hitPoint = rayPoint(ray, dist);
		if (primId == -1)
		{
			result = envGetRadiance(ray.dir);
		}
		else if (primId < uObjPrimCount)
		{
			ray.ori = hitPoint;
			SurfaceInfo sInfo = triangleSurfaceInfo(primId, ray.ori);
			result = uAoMode ? aoIntegTrace(ray, primId, sInfo, s) : pathIntegTrace(ray, primId, sInfo, s);
		}
		else
		{
			result = lightGetRadiance(primId - uObjPrimCount, hitPoint, -ray.dir);
		}
	}
	return result;
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	vec2 texSize = vec2(uFilmSize);

	if (coord.x >= uFilmSize.x || coord.y >= uFilmSize.y) return;

	vec2 scrCoord = vec2(coord) / texSize;

	int sampleIdx = 0;
	
	vec2 noiseCoord = texture(uNoiseTex, scrCoord).xy;
	noiseCoord = texture(uNoiseTex, noiseCoord).xy;
	vec2 texCoord = texSize * noiseCoord;
	randSeed = (uint(texCoord.x) * uFreeCounter) + uint(texCoord.y);
	sampleSeed = uint(texCoord.x) * uint(texCoord.y);

	//sampleOffset = uSpp + int(texCoord.x) + int(texCoord.y);
	sampleOffset = uSpp * uSampleDim;
	if (sampleOffset > uSampleNum * uSampleDim) sampleOffset -= uSampleNum * uSampleDim;

	Ray ray = sampleLensCamera(scrCoord, sampleIdx);
	vec3 result = getResult(ray, sampleIdx);
	if (BUG) result = BUGVAL;

	vec3 lastRes = (uSpp == 0) ? vec3(0.0) : imageLoad(uFrame, coord).rgb;

	if (!hasNan(result))
		imageStore(uFrame, coord, vec4(lastRes + result, 1.0));
}