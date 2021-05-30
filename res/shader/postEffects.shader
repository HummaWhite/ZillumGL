=type vertex
#version 450 core
layout(location = 0) in vec2 aTexCoord;
out vec2 texCoord;

void main()
{
	texCoord = aTexCoord;
	vec2 transTex = aTexCoord * 2.0 - 1.0;
	gl_Position = vec4(transTex, 0.0, 1.0);
}

=type fragment
#version 450 core
in vec2 texCoord;
out vec4 FragColor;

uniform sampler2D frameBuffer;
uniform int toneMapping;

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
	vec2 texPos = texCoord;
	vec3 color = texture(frameBuffer, texPos).rgb;
	
	// 如果不Clamp掉负值会出现奇怪的问题
	color = clamp(color, 0.0, 1e8);

	vec3 mapped = color;
	switch (toneMapping)
	{
	case 1:
		mapped = reinhard(color);
		break;
	case 2:
		mapped = filmic(color);
		break;
	case 3:
		mapped = ACES(color);
		break;
	}

	mapped = pow(mapped, vec3(1.0 / 2.2));
	FragColor = vec4(mapped, 1.0);
}