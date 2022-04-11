@type vertex
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec3 aNorm;

out vec3 vPos;
out vec2 vUV;
out vec3 vNorm;

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
in vec3 vPos;
in vec2 vUV;
in vec3 vNorm;

out vec4 FragColor;

uniform sampler2DArray uTextures;
uniform vec3 uCamPos;
uniform int uTexIndex;
uniform float uTexScale;
uniform int uChannel;

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

void main()
{
	vec3 result;
	vec3 wi = normalize(uCamPos - vPos);
	vec3 baseColor = vec3(1.0);
	switch (uChannel)
	{
	case 0:
		result = baseColor * abs(dot(vNorm, wi));
		result = filmic(result);
		break;
	case 1:
		result = vPos;
		break;
	case 2:
		result = (vNorm + 1.0) * 0.5;
	}
	FragColor = vec4(pow(result, vec3(1.0 / 2.2)), 1.0);
}