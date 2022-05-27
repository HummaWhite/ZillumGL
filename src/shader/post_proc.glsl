@type compute

layout(rgba32f, binding = 6) uniform writeonly image2D uOut;

uniform sampler2D uIn;
uniform int uToneMapper;
uniform float uResultScale;
uniform ivec2 uFilmSize;
uniform bool uPreview;
uniform int uPreviewScale;

vec3 reinhard(vec3 color)
{
	return color / (color + 1.0);
}

vec3 CE(vec3 color)
{
	return 1.0 - exp(-color);
}

vec3 calc(vec3 x)
{
	const float A = 0.22, B = 0.3, C = 0.1, D = 0.2, E = 0.01, F = 0.3;
	return (x * (x * A + B * C) + D * E) / (x * (x * A + B) + D * F) - E / F;
}

vec3 filmic(vec3 color)
{
	const float WHITE = 11.2;
	return calc(color * 1.6) / calc(vec3(WHITE));
}

vec3 ACES(vec3 color)
{
	return (color * (color * 2.51 + 0.03f)) / (color * (color * 2.43 + 0.59) + 0.14);
}

void main()
{
	ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
	if (coord.x >= uFilmSize.x || coord.y >= uFilmSize.y)
		return;
	vec3 color = texelFetch(uIn, coord / (uPreview ? uPreviewScale : 1), 0).rgb * uResultScale;

	color = clamp(color, 0.0, 1e30);

	vec3 mapped = color;
	switch (uToneMapper)
	{
	case 1:
		mapped = filmic(color);
		break;
	case 2:
		mapped = ACES(color);
		break;
	}
	mapped = pow(mapped, vec3(1.0 / 2.2));
	imageStore(uOut, coord, vec4(mapped, 1.0));
}