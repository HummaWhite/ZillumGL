@type compute

layout(rgba32f, binding = 0) uniform image2D uFrame;

uniform int uMatIndex;
uniform float uBvhDepth;

@include random.glsl
@include math.glsl
@include intersection.glsl
@include camera.glsl

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	vec2 texSize = vec2(uFilmSize);

	if (coord.x >= uFilmSize.x || coord.y >= uFilmSize.y)
		return;

	vec3 result;
	Sampler sampleIdx = 0;
	Ray ray = thinLensCameraSampleRay(scrCoord, sampleIdx);

	float maxDepth;
	int primId = bvhDebug(ray, maxDepth);
	int matId = texelFetch(uMatTexIndices, primId).r & 0xffff;
	if (matId == uMatIndex && primId != -1) result = vec3(1.0, 1.0, 0.2);
	else result = vec3(maxDepth / uBvhDepth * 0.2);

	imageStore(uFrame, coord, vec4(result, 1.0));
}