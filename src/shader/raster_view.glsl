@type vertex
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNorm;

layout(location = 0) out vec3 vPos;
layout(location = 1) out vec2 vUV;
layout(location = 2) out vec3 vNorm;

uniform mat4 uMVPMatrix;
uniform mat4 uModelMat;
uniform mat3 uModelInv;

void main()
{
	gl_Position = uMVPMatrix * vec4(aPos, 1.0);
	vPos = vec3(uModelMat * vec4(aPos, 1.0));
	vNorm = normalize(uModelInv * aNorm);
	vUV = aUV;
}

@type fragment
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vNorm;

layout(location = 0) out vec4 FragColor;

uniform sampler2DArray uTextures;
uniform vec3 uCamPos;
uniform vec3 uBaseColor;
uniform int uTexIndex;
uniform vec2 uTexScale;
uniform int uChannel;

uniform bool uHighlightMaterial;
uniform bool uHighlightModel;
uniform int uCounter;

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

float maxComponent(vec3 v)
{
	return max(v.x, max(v.y, v.z));
}

void main()
{
	vec3 result;
	vec3 wi = normalize(uCamPos - vPos);
	if (uChannel == 0)
	{
		vec3 baseColor = (uTexIndex == -1) ? uBaseColor :
			texture(uTextures, vec3(fract(vUV) * uTexScale, uTexIndex)).rgb;
		result = baseColor * abs(dot(vNorm, wi));
		result = filmic(result);
	}
	else if (uChannel == 1)
		result = vPos;
	else if (uChannel == 2)
		result = (vNorm + 1.0) * 0.5;
	else if (uChannel == 3)
		result = vec3(fract(vUV) * ((uTexIndex == -1) ? vec2(1.0) : uTexScale), 1.0);

	ivec2 uv = ivec2(gl_FragCoord.xy) + ivec2(1, 0) * uCounter;

	if (uHighlightModel && (uv.x + uv.y) / 40 % 2 == 0)
		result += vec3(1.0, 0.4, 0.2);
	if (uHighlightMaterial && (uv.x + uv.y) / 40 % 2 == 1)
		result += vec3(0.2, 0.2, 0.8);

	result = min(result, vec3(1.0));
	FragColor = vec4(pow(result, vec3(1.0 / 2.2)), 1.0);
}